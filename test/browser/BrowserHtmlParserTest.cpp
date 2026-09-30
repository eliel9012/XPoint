#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "browser/BrowserHtmlParser.h"

using xpoint::browser::BrowserDocument;
using xpoint::browser::BrowserHtmlParser;

static void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::abort();
  }
}

static void feed(BrowserHtmlParser& parser, const std::string& html) {
  // Every input byte is a chunk boundary, including style and script syntax.
  for (const char ch : html) check(parser.feed(reinterpret_cast<const uint8_t*>(&ch), 1), "feed failed");
  check(parser.finish(), "finish failed");
}

int main() {
  BrowserDocument document;
  BrowserHtmlParser parser;

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<div>one <span style='text-transform: uppercase; color: red'>mIx</span> "
       "<span style='text-transform: lowercase'>CASE</span> end</div>");
  check(std::string(document.text.c_str()).find("one MIX case end") != std::string::npos,
        "inline text transform or inherited state");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "A<div style='display:none'>secret <a href='/hidden'>link</a></div>"
       "<span style='white-space:pre'>  B\n  C</span>D");
  check(std::string(document.text.c_str()) == "A  B\n  CD", "hidden text or pre whitespace");
  check(document.links.empty(), "hidden link leaked");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<a href='/next' style='text-transform:uppercase'>two words</a>"
       "<script>document.write(' and JS\\ntext');</script>"
       "<script>eval('bad')</script>"
       "<script>document.write('a' + 'b')</script>"
       "<script src='/external.js'>document.write('bad')</script>"
       "<script>if (1 < 2) document.write('bad')</script>"
       "<script>document.write('bad\nnewline')</script>"
       "<script type='module'>document.write('bad')</script>");
  check(std::string(document.text.c_str()) == "TWO WORDS and JS text", "literal script or unsupported script");
  check(document.links.size() == 1 && std::string(document.links[0].text.c_str()) == "TWO WORDS",
        "link label transform and whitespace");

  parser.reset(document, "https://example.org/");
  std::string manyScripts;
  for (int i = 0; i < 9; ++i) manyScripts += "<script>document.write('x')</script>";
  feed(parser, manyScripts);
  check(std::string(document.text.c_str()) == "xxxxxxxx", "script count bound");

  parser.reset(document, "https://example.org/");
  feed(parser, "<script>document.write('" + std::string(200, 'x') + "')</script>OK");
  check(std::string(document.text.c_str()) == "OK", "script output bound");

  parser.reset(document, "https://example.org/");
  feed(parser, "<script>document.write('" + std::string(300, 'x') + "')</script>OK");
  check(std::string(document.text.c_str()) == "OK", "script source bound");

  parser.reset(document, "https://example.org/");
  feed(parser, "<script>document.write(\"hello\")</script >there");
  check(std::string(document.text.c_str()) == "hellothere", "quoted literal and closing whitespace");

  parser.reset(document, "https://example.org/");
  feed(parser, "<span style='text-transform:uppercase'>A<span style='text-transform:none'>b</span>c</span>d");
  check(std::string(document.text.c_str()) == "AbCd", "nested style restoration");

  parser.reset(document, "https://example.org/");
  feed(parser, "<title style='display:none;text-transform:uppercase'>Mixed Title</title><p>body</p>");
  check(std::string(document.title.c_str()) == "Mixed Title", "title must ignore presentation styles");
  check(std::string(document.text.c_str()) == "body\n", "title leaked into body");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style type='text/css'>/* top */ SPAN{text-transform:uppercase}"
       ".quiet{display:none}.upper{text-transform:uppercase}"
       "#keep{white-space:pre;text-transform:none} .lower{text-transform:lowercase}</style>"
       "<span>ab</span><span class='x quiet y'>hidden<a href='/hidden'>link</a></span>"
       "<span class='upper lower'>MiX</span>"
       "<span id='keep'>  c\n d</span>"
       "<span class='lower' style='text-transform:none'>MiX</span>");
  check(std::string(document.text.c_str()) == "ABmix  c\n dMiX", "style selectors, order, inline precedence");
  check(document.links.empty(), "stylesheet hidden link leaked");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style>.x{text-transform:uppercase}.x{text-transform:lowercase}"
       "#id{text-transform:uppercase}span{text-transform:lowercase}</style>"
       "<span class='x'>MiX</span><span id='id' class='x'>MiX</span>"
       "<span style='text-transform:none' id='id'>MiX</span>"
       "<style>.x{text-transform:uppercase}</style><span class='x'>MiX</span>");
  check(std::string(document.text.c_str()) == "mixMIXMiXMIX", "specificity and later sheet order");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style>@media screen {.bad{display:none}}"
       "span.bad{display:none}.bad,.other{display:none}"
       "/* gap */ .good{text-transform:uppercase}"
       ".comment{display:none;/* } */text-transform:uppercase}"
       ".quote{content:'x;display:none';text-transform:uppercase}</style>"
       "<span class='bad'>A</span><span class='good'>b</span>"
       "<span class='comment'>c</span><span class='quote'>d</span>"
       "<span style='content:"
       "\"x;display:none;\";text-transform:uppercase'>e</span>");
  check(std::string(document.text.c_str()) == "ABcde", "unsupported CSS must not escape subset");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "before<style>.x{text-transform:uppercase}x<ignored>y</style>"
       "<span class='x'>MiX</span>after"
       "<style type='text/less'>.x{display:none}</style>"
       "<style>.x{display:none}</style><span class='x'>still</span>");
  check(std::string(document.text.c_str()) == "beforeMiXafterstill", "invalid sheet and sheet count bound");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style>.x{display:none}</style><span class='x'>first</span>"
       "<style>.x{display:none}</style><span class='x'>second</span>");
  check(document.text.empty(), "style state after reset");

  parser.reset(document, "https://example.org/");
  feed(parser, "<style>" + std::string(512, ' ') + ".x{display:none}</style><span class='x'>visible</span>");
  check(std::string(document.text.c_str()) == "visible", "style source bound");

  parser.reset(document, "https://example.org/");
  std::string manyRules = "<style>";
  for (int i = 0; i < 9; ++i) manyRules += ".r" + std::to_string(i) + "{display:none}";
  manyRules += "</style><span class='r7'>hidden</span><span class='r8'>shown</span>";
  feed(parser, manyRules);
  check(std::string(document.text.c_str()) == "shown", "style rule count bound");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style>.longselectorname1234567890123456{display:none}</style >"
       "<span class='longselectorname1234567890123456'>visible</span>");
  check(std::string(document.text.c_str()) == "visible", "selector length and close whitespace");

  parser.reset(document, "https://example.org/");
  feed(parser, "old<style>.x{text-transform:uppercase}</style><span class='x'>new</span>");
  check(std::string(document.text.c_str()) == "oldNEW", "streaming styles are not retroactive");

  parser.reset(document, "https://example.org/");
  feed(parser,
       "<style>.outer{display:none}.pre{white-space:pre}</style>"
       "<span class='outer'>x<span style='display:block'>y</span></span>"
       "<span class='pre'> a<span style='white-space:normal'>  b</span> c</span>");
  check(std::string(document.text.c_str()) == " a b c", "hidden inheritance and whitespace override");

  parser.reset(document, "https://example.org/");
  std::string deep;
  for (int i = 0; i < 65; ++i) deep += "<span>";
  deep += "<span style='display:none'>visible</span>";
  for (int i = 0; i < 65; ++i) deep += "</span>";
  deep += "tail";
  feed(parser, deep);
  check(std::string(document.text.c_str()) == "visibletail", "deep style must degrade without dropping text");

  std::cout << "browser parser tests passed\n";
}
