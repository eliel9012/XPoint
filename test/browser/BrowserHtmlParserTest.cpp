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
  std::string deep;
  for (int i = 0; i < 65; ++i) deep += "<span>";
  deep += "<span style='display:none'>visible</span>";
  for (int i = 0; i < 65; ++i) deep += "</span>";
  deep += "tail";
  feed(parser, deep);
  check(std::string(document.text.c_str()) == "visibletail", "deep style must degrade without dropping text");

  std::cout << "browser parser tests passed\n";
}
