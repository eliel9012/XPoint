#include "BrowserHtmlParser.h"

#include <cctype>
#include <cstring>

namespace xpoint::browser {
namespace {

bool equalsIgnoreCase(const char* value, size_t length, const char* expected) {
  const size_t expectedLength = std::strlen(expected);
  if (length != expectedLength) return false;
  for (size_t i = 0; i < length; ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(expected[i])))
      return false;
  }
  return true;
}

bool startsWithIgnoreCase(const char* value, size_t length, const char* expected) {
  const size_t expectedLength = std::strlen(expected);
  if (length < expectedLength) return false;
  return equalsIgnoreCase(value, expectedLength, expected);
}

bool isBlockTag(const char* name, size_t length) {
  return equalsIgnoreCase(name, length, "p") || equalsIgnoreCase(name, length, "div") ||
         equalsIgnoreCase(name, length, "br") || equalsIgnoreCase(name, length, "li") ||
         equalsIgnoreCase(name, length, "h1") || equalsIgnoreCase(name, length, "h2") ||
         equalsIgnoreCase(name, length, "h3") || equalsIgnoreCase(name, length, "h4") ||
         equalsIgnoreCase(name, length, "section") || equalsIgnoreCase(name, length, "article") ||
         equalsIgnoreCase(name, length, "tr") || equalsIgnoreCase(name, length, "blockquote") ||
         equalsIgnoreCase(name, length, "hr");
}

bool isVoidTag(const char* name, size_t length) {
  return equalsIgnoreCase(name, length, "area") || equalsIgnoreCase(name, length, "base") ||
         equalsIgnoreCase(name, length, "br") || equalsIgnoreCase(name, length, "col") ||
         equalsIgnoreCase(name, length, "embed") || equalsIgnoreCase(name, length, "hr") ||
         equalsIgnoreCase(name, length, "img") || equalsIgnoreCase(name, length, "input") ||
         equalsIgnoreCase(name, length, "link") || equalsIgnoreCase(name, length, "meta") ||
         equalsIgnoreCase(name, length, "param") || equalsIgnoreCase(name, length, "source") ||
         equalsIgnoreCase(name, length, "track") || equalsIgnoreCase(name, length, "wbr");
}

uint32_t nameHash(const char* name, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) {
    hash ^= static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(name[i])));
    hash *= 16777619u;
  }
  return hash;
}

void trim(const char*& value, size_t& length) {
  while (length != 0 && std::isspace(static_cast<unsigned char>(*value))) {
    ++value;
    --length;
  }
  while (length != 0 && std::isspace(static_cast<unsigned char>(value[length - 1]))) --length;
}

bool isClosingTag(const char* tag, size_t length) { return length > 1 && tag[0] == '<' && tag[1] == '/'; }

void tagName(const char* tag, size_t length, const char*& name, size_t& nameLength) {
  size_t start = (length != 0 && tag[0] == '<') ? 1 : 0;
  if (start < length && tag[start] == '/') ++start;
  while (start < length && std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
  const size_t end = start + [&] {
    size_t i = start;
    while (i < length && !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>' && tag[i] != '/') ++i;
    return i - start;
  }();
  name = tag + start;
  nameLength = end - start;
}

bool findAttribute(const char* tag, size_t length, const char* attribute, const char*& value, size_t& valueLength) {
  size_t i = 1;
  if (i < length && tag[i] == '/') ++i;
  // Skip the element name. Attribute scanning starts at the first token after
  // it; otherwise a tag such as <a href="..."> consumes href as part of the
  // valueless element name.
  while (i < length && !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>' && tag[i] != '/') ++i;
  while (i < length) {
    while (i < length && (std::isspace(static_cast<unsigned char>(tag[i])) || tag[i] == '>')) ++i;
    if (i >= length || tag[i] == '/') break;
    const size_t nameStart = i;
    while (i < length && !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '=' && tag[i] != '>') ++i;
    const size_t nameLength = i - nameStart;
    const size_t afterName = i;
    while (i < length && std::isspace(static_cast<unsigned char>(tag[i]))) ++i;
    if (i >= length || tag[i] != '=') {
      i = afterName;
      continue;
    }
    ++i;
    while (i < length && std::isspace(static_cast<unsigned char>(tag[i]))) ++i;
    if (i >= length) break;
    const char quote = (tag[i] == '\'' || tag[i] == '"') ? tag[i++] : '\0';
    const size_t valueStart = i;
    if (quote != '\0') {
      while (i < length && tag[i] != quote) ++i;
    } else {
      while (i < length && !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>') ++i;
    }
    if (equalsIgnoreCase(tag + nameStart, nameLength, attribute)) {
      value = tag + valueStart;
      valueLength = i - valueStart;
      return true;
    }
    if (quote != '\0' && i < length) ++i;
  }
  return false;
}

bool hasScheme(const char* value, size_t length) {
  for (size_t i = 0; i + 2 < length; ++i) {
    if (value[i] == ':' && value[i + 1] == '/' && value[i + 2] == '/') return i != 0;
    if (value[i] == '/' || value[i] == '?' || value[i] == '#') break;
  }
  return false;
}

bool supportedLink(const char* value, size_t length) {
  if (length == 0 || value[0] == '#') return false;
  if (length >= 7 && startsWithIgnoreCase(value, length, "mailto:")) return false;
  if (length >= 11 && startsWithIgnoreCase(value, length, "javascript:")) return false;
  return true;
}

bool resolveUrl(const char* base, size_t baseLength, const char* href, size_t hrefLength,
                FixedString<kMaxLinkUrlLength>& output) {
  if (!supportedLink(href, hrefLength)) return false;
  if (hasScheme(href, hrefLength)) return output.assign(href, hrefLength);

  FixedString<kMaxLinkUrlLength> result;
  if (hrefLength >= 2 && href[0] == '/' && href[1] == '/') {
    size_t schemeEnd = 0;
    while (schemeEnd + 2 < baseLength && base[schemeEnd] != ':') ++schemeEnd;
    if (schemeEnd + 2 < baseLength) {
      result.append(base, schemeEnd + 1);
      result.append(href, hrefLength);
    }
  } else if (hrefLength != 0 && href[0] == '/') {
    size_t hostEnd = 0;
    while (hostEnd + 2 < baseLength && base[hostEnd] != ':') ++hostEnd;
    hostEnd = hostEnd + 3 < baseLength ? hostEnd + 3 : baseLength;
    while (hostEnd < baseLength && base[hostEnd] != '/') ++hostEnd;
    result.append(base, hostEnd);
    result.append(href, hrefLength);
  } else {
    size_t pathEnd = baseLength;
    while (pathEnd > 0 && base[pathEnd - 1] != '/') --pathEnd;
    if (pathEnd == 0) {
      result.append(base, baseLength);
      result.append('/');
    } else {
      result.append(base, pathEnd);
    }
    result.append(href, hrefLength);
  }
  if (result.empty()) return false;
  return output.assign(result.c_str());
}

constexpr uint8_t kDisplayProperty = 1;
constexpr uint8_t kWhitespaceProperty = 2;
constexpr uint8_t kTransformProperty = 4;

bool isCssNameChar(const unsigned char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
}

}  // namespace

void BrowserHtmlParser::reset(BrowserDocument& document, const char* pageUrl) {
  document.clear();
  document.url.assign(pageUrl);
  tagLength_ = 0;
  inTag_ = false;
  quote_ = '\0';
  inComment_ = false;
  skipText_ = false;
  inTitle_ = false;
  inLink_ = false;
  trackedLink_ = false;
  pendingSpace_ = false;
  titlePendingSpace_ = false;
  failed_ = false;
  entityLength_ = 0;
  inEntity_ = false;
  currentLink_ = {};
  elementDepth_ = 0;
  overflowDepth_ = 0;
  presentation_ = {};
  scriptLength_ = 0;
  scriptCount_ = 0;
  inScript_ = false;
  scriptEligible_ = false;
  scriptCloseMatch_ = 0;
  styleLength_ = 0;
  styleSheetCount_ = 0;
  inStyle_ = false;
  styleEligible_ = false;
  styleCloseMatch_ = 0;
  ruleCount_ = 0;
  document_ = &document;
}

bool BrowserHtmlParser::appendTagChar(const char value) {
  if (tagLength_ + 1 >= kTagBufferSize) {
    failed_ = true;
    return false;
  }
  tag_[tagLength_++] = value;
  tag_[tagLength_] = '\0';
  return true;
}

bool BrowserHtmlParser::appendEntityChar(const char value) {
  if (entityLength_ + 1 >= kEntityBufferSize) return false;
  entity_[entityLength_++] = value;
  entity_[entityLength_] = '\0';
  return true;
}

void BrowserHtmlParser::emitLinkText(const char value) {
  if (!trackedLink_ || value == '\n' || value == '\r') return;
  if (std::isspace(static_cast<unsigned char>(value))) {
    if (!currentLink_.text.empty()) currentLink_.text.append(' ');
  } else {
    currentLink_.text.append(value);
  }
}

void BrowserHtmlParser::emitTextChar(const char value) {
  if (document_ == nullptr || skipText_) return;
  if (inTitle_) {
    if (std::isspace(static_cast<unsigned char>(value))) {
      titlePendingSpace_ = !document_->title.empty();
    } else {
      if (titlePendingSpace_ && !document_->title.empty()) document_->title.append(' ');
      document_->title.append(value);
      titlePendingSpace_ = false;
    }
    return;
  }
  if (presentation_.hidden) return;
  if (presentation_.preserveWhitespace) {
    if (pendingSpace_ && !document_->text.empty() && document_->text[document_->text.size() - 1] != '\n' &&
        !document_->text.append(' '))
      document_->textTruncated = true;
    pendingSpace_ = false;
    const char output = value == '\t' ? ' ' : (value == '\r' ? '\n' : value);
    const unsigned char byte = static_cast<unsigned char>(output);
    char transformed = output;
    if (byte < 128 && presentation_.transform == Transform::Uppercase)
      transformed = static_cast<char>(std::toupper(byte));
    else if (byte < 128 && presentation_.transform == Transform::Lowercase)
      transformed = static_cast<char>(std::tolower(byte));
    if (!document_->text.append(transformed)) document_->textTruncated = true;
    emitLinkText(transformed);
    return;
  }
  if (std::isspace(static_cast<unsigned char>(value))) {
    pendingSpace_ = !document_->text.empty();
    emitLinkText(value);
    return;
  }
  if (pendingSpace_ && !document_->text.empty() && document_->text[document_->text.size() - 1] != '\n') {
    if (!document_->text.append(' ')) document_->textTruncated = true;
  }
  pendingSpace_ = false;
  const unsigned char byte = static_cast<unsigned char>(value);
  char transformed = value;
  if (byte < 128 && presentation_.transform == Transform::Uppercase)
    transformed = static_cast<char>(std::toupper(byte));
  else if (byte < 128 && presentation_.transform == Transform::Lowercase)
    transformed = static_cast<char>(std::tolower(byte));
  if (!document_->text.append(transformed)) document_->textTruncated = true;
  emitLinkText(transformed);
}

void BrowserHtmlParser::emitText(const char* value, const size_t length) {
  for (size_t i = 0; i < length; ++i) emitTextChar(value[i]);
}

void BrowserHtmlParser::emitBlockBreak() {
  if (document_ == nullptr || presentation_.hidden || document_->text.empty()) return;
  pendingSpace_ = false;
  if (document_->text[document_->text.size() - 1] != '\n' && !document_->text.append('\n'))
    document_->textTruncated = true;
}

void BrowserHtmlParser::pushElement(const char* name, const size_t nameLength, const char* tag,
                                    const size_t tagLength) {
  if (elementDepth_ == kMaxElementDepth || overflowDepth_ != 0) {
    if (overflowDepth_ != static_cast<size_t>(-1)) ++overflowDepth_;
    return;
  }
  TextPresentation next = presentation_;
  uint8_t displaySpecificity = 0;
  uint8_t whitespaceSpecificity = 0;
  uint8_t transformSpecificity = 0;
  for (size_t i = 0; i < ruleCount_; ++i) {
    const CssRule& rule = rules_[i];
    if (!matchesRule(rule, name, nameLength, tag, tagLength)) continue;
    const CssDeclarations& declarations = rule.declarations;
    if ((declarations.properties & kDisplayProperty) && rule.specificity >= displaySpecificity) {
      next.hidden = next.hidden || declarations.presentation.hidden;
      displaySpecificity = rule.specificity;
    }
    if ((declarations.properties & kWhitespaceProperty) && rule.specificity >= whitespaceSpecificity) {
      next.preserveWhitespace = declarations.presentation.preserveWhitespace;
      whitespaceSpecificity = rule.specificity;
    }
    if ((declarations.properties & kTransformProperty) && rule.specificity >= transformSpecificity) {
      next.transform = declarations.presentation.transform;
      transformSpecificity = rule.specificity;
    }
  }
  const char* style = nullptr;
  size_t styleLength = 0;
  if (findAttribute(tag, tagLength, "style", style, styleLength))
    applyDeclarations(next, parseDeclarations(style, styleLength));
  elements_[elementDepth_++] = {nameHash(name, nameLength), next};
  presentation_ = next;
}

BrowserHtmlParser::CssDeclarations BrowserHtmlParser::parseDeclarations(const char* style, const size_t length) {
  CssDeclarations result;
  // Reject syntax needing a CSS tokenizer. In particular, a semicolon inside
  // a quoted value must never become a new supported declaration.
  for (size_t i = 0; i < length; ++i) {
    const char ch = style[i];
    if (ch == '\0' || ch == '\'' || ch == '"' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '/' ||
        ch == '\\' || ch == '<' || ch == '>')
      return result;
  }
  size_t cursor = 0;
  while (cursor < length) {
    const size_t start = cursor;
    while (cursor < length && style[cursor] != ';') ++cursor;
    const size_t end = cursor++;
    size_t colon = start;
    while (colon < end && style[colon] != ':') ++colon;
    if (colon == end) continue;
    const char* property = style + start;
    size_t propertyLength = colon - start;
    const char* value = style + colon + 1;
    size_t valueLength = end - colon - 1;
    trim(property, propertyLength);
    trim(value, valueLength);
    if (equalsIgnoreCase(property, propertyLength, "display") && equalsIgnoreCase(value, valueLength, "none")) {
      result.properties |= kDisplayProperty;
      result.presentation.hidden = true;
    } else if (equalsIgnoreCase(property, propertyLength, "white-space")) {
      if (equalsIgnoreCase(value, valueLength, "pre") || equalsIgnoreCase(value, valueLength, "normal")) {
        result.properties |= kWhitespaceProperty;
        result.presentation.preserveWhitespace = equalsIgnoreCase(value, valueLength, "pre");
      }
    } else if (equalsIgnoreCase(property, propertyLength, "text-transform")) {
      if (equalsIgnoreCase(value, valueLength, "uppercase")) {
        result.properties |= kTransformProperty;
        result.presentation.transform = Transform::Uppercase;
      } else if (equalsIgnoreCase(value, valueLength, "lowercase")) {
        result.properties |= kTransformProperty;
        result.presentation.transform = Transform::Lowercase;
      } else if (equalsIgnoreCase(value, valueLength, "none")) {
        result.properties |= kTransformProperty;
        result.presentation.transform = Transform::None;
      }
    }
  }
  return result;
}

void BrowserHtmlParser::applyDeclarations(TextPresentation& target, const CssDeclarations& declarations) {
  if (declarations.properties & kDisplayProperty) target.hidden = true;
  if (declarations.properties & kWhitespaceProperty)
    target.preserveWhitespace = declarations.presentation.preserveWhitespace;
  if (declarations.properties & kTransformProperty) target.transform = declarations.presentation.transform;
}

bool BrowserHtmlParser::matchesRule(const CssRule& rule, const char* name, const size_t nameLength, const char* tag,
                                    const size_t tagLength) {
  if (rule.selector[0] != '.' && rule.selector[0] != '#') return equalsIgnoreCase(name, nameLength, rule.selector);
  const char* attribute = nullptr;
  size_t length = 0;
  if (!findAttribute(tag, tagLength, rule.selector[0] == '.' ? "class" : "id", attribute, length)) return false;
  const char* expected = rule.selector + 1;
  const size_t expectedLength = rule.selectorLength - 1;
  if (rule.selector[0] == '#') return length == expectedLength && std::memcmp(attribute, expected, length) == 0;
  size_t cursor = 0;
  while (cursor < length) {
    while (cursor < length && std::isspace(static_cast<unsigned char>(attribute[cursor]))) ++cursor;
    const size_t start = cursor;
    while (cursor < length && !std::isspace(static_cast<unsigned char>(attribute[cursor]))) ++cursor;
    if (cursor - start == expectedLength && std::memcmp(attribute + start, expected, expectedLength) == 0) return true;
  }
  return false;
}

void BrowserHtmlParser::parseStyleSheet() {
  if (!styleEligible_) return;
  size_t cursor = 0;
  while (cursor < styleLength_ && ruleCount_ < kMaxStyleRules) {
    while (cursor < styleLength_ && std::isspace(static_cast<unsigned char>(style_[cursor]))) ++cursor;
    if (cursor + 1 < styleLength_ && style_[cursor] == '/' && style_[cursor + 1] == '*') {
      cursor += 2;
      while (cursor + 1 < styleLength_ && !(style_[cursor] == '*' && style_[cursor + 1] == '/')) ++cursor;
      if (cursor + 1 >= styleLength_) return;
      cursor += 2;
      continue;
    }
    const size_t selectorStart = cursor;
    while (cursor < styleLength_ && style_[cursor] != '{' && style_[cursor] != '}') ++cursor;
    if (cursor == styleLength_) return;
    if (style_[cursor] == '}') return;
    const char* selector = style_ + selectorStart;
    size_t selectorLength = cursor - selectorStart;
    trim(selector, selectorLength);
    const size_t declarationStart = ++cursor;
    size_t depth = 1;
    char quote = '\0';
    bool nested = false;
    while (cursor < styleLength_ && depth != 0) {
      const char ch = style_[cursor++];
      if (quote != '\0') {
        if (ch == '\\' && cursor < styleLength_)
          ++cursor;
        else if (ch == quote)
          quote = '\0';
      } else if (ch == '/' && cursor < styleLength_ && style_[cursor] == '*') {
        ++cursor;
        while (cursor + 1 < styleLength_ && !(style_[cursor] == '*' && style_[cursor + 1] == '/')) ++cursor;
        if (cursor + 1 >= styleLength_) return;
        cursor += 2;
      } else if (ch == '\'' || ch == '"') {
        quote = ch;
      } else if (ch == '{') {
        nested = true;
        ++depth;
      } else if (ch == '}') {
        --depth;
      }
    }
    if (depth != 0) return;
    const size_t declarationLength = cursor - declarationStart - 1;
    if (nested) continue;
    if (selectorLength == 0 || selectorLength >= kSelectorSize) continue;
    const size_t nameStart = (selector[0] == '.' || selector[0] == '#') ? 1 : 0;
    if (nameStart == selectorLength) continue;
    const unsigned char first = static_cast<unsigned char>(selector[nameStart]);
    bool simple = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_';
    for (size_t i = nameStart; i < selectorLength; ++i) {
      const unsigned char ch = static_cast<unsigned char>(selector[i]);
      if (!isCssNameChar(ch)) simple = false;
    }
    if (!simple) continue;
    CssRule& rule = rules_[ruleCount_];
    std::memcpy(rule.selector, selector, selectorLength);
    rule.selector[selectorLength] = '\0';
    rule.selectorLength = static_cast<uint8_t>(selectorLength);
    rule.specificity = selector[0] == '#' ? 3 : (selector[0] == '.' ? 2 : 1);
    rule.declarations = parseDeclarations(style_ + declarationStart, declarationLength);
    if (rule.declarations.properties != 0) ++ruleCount_;
  }
}

void BrowserHtmlParser::closeElement(const char* name, const size_t nameLength) {
  if (overflowDepth_ != 0) {
    --overflowDepth_;
    return;
  }
  const uint32_t hash = nameHash(name, nameLength);
  for (size_t i = elementDepth_; i != 0; --i) {
    if (elements_[i - 1].nameHash != hash) continue;
    elementDepth_ = i - 1;
    presentation_ = elementDepth_ == 0 ? TextPresentation{} : elements_[elementDepth_ - 1].presentation;
    return;
  }
}

void BrowserHtmlParser::runLiteralScript() {
  if (!scriptEligible_ || scriptLength_ == 0 || document_ == nullptr) return;
  constexpr char kCall[] = "document.write";
  size_t i = 0;
  const auto skipSpaces = [&] {
    while (i < scriptLength_ && std::isspace(static_cast<unsigned char>(script_[i]))) ++i;
  };
  skipSpaces();
  if (scriptLength_ - i < sizeof(kCall) - 1 || std::memcmp(script_ + i, kCall, sizeof(kCall) - 1) != 0) return;
  i += sizeof(kCall) - 1;
  skipSpaces();
  if (i == scriptLength_ || script_[i++] != '(') return;
  skipSpaces();
  if (i == scriptLength_ || (script_[i] != '\'' && script_[i] != '"')) return;
  const char quote = script_[i++];
  char output[161]{};
  size_t outputLength = 0;
  bool closed = false;
  while (i < scriptLength_) {
    char ch = script_[i++];
    if (ch == quote) {
      closed = true;
      break;
    }
    if (ch == '\0' || ch == '\n' || ch == '\r') return;
    if (ch == '\\') {
      if (i == scriptLength_) return;
      const char escape = script_[i++];
      if (escape == 'n')
        ch = '\n';
      else if (escape == 't')
        ch = '\t';
      else if (escape == 'r')
        ch = '\r';
      else if (escape == quote || escape == '\\')
        ch = escape;
      else
        return;
    }
    if (outputLength == sizeof(output) - 1) return;
    output[outputLength++] = ch;
  }
  if (!closed) return;
  skipSpaces();
  if (i == scriptLength_ || script_[i++] != ')') return;
  skipSpaces();
  if (i < scriptLength_ && script_[i] == ';') ++i;
  skipSpaces();
  if (i != scriptLength_) return;
  emitText(output, outputLength);
}

void BrowserHtmlParser::processEntity() {
  if (!inEntity_) return;
  if (equalsIgnoreCase(entity_, entityLength_, "amp"))
    emitTextChar('&');
  else if (equalsIgnoreCase(entity_, entityLength_, "lt"))
    emitTextChar('<');
  else if (equalsIgnoreCase(entity_, entityLength_, "gt"))
    emitTextChar('>');
  else if (equalsIgnoreCase(entity_, entityLength_, "quot"))
    emitTextChar('"');
  else if (equalsIgnoreCase(entity_, entityLength_, "apos") || equalsIgnoreCase(entity_, entityLength_, "#39"))
    emitTextChar('\'');
  else if (equalsIgnoreCase(entity_, entityLength_, "nbsp"))
    emitTextChar(' ');
  else {
    emitTextChar('&');
    emitText(entity_, entityLength_);
    emitTextChar(';');
  }
  entityLength_ = 0;
  inEntity_ = false;
}

void BrowserHtmlParser::beginLink(const char* href, const size_t length) {
  currentLink_ = {};
  trackedLink_ = false;
  if (document_ == nullptr || presentation_.hidden) return;
  if (document_->links.full()) {
    document_->linksTruncated = true;
    return;
  }
  if (!resolveUrl(document_->url.c_str(), document_->url.size(), href, length, currentLink_.url)) return;
  trackedLink_ = true;
}

void BrowserHtmlParser::endLink() {
  if (trackedLink_ && document_ != nullptr && !document_->links.push_back(currentLink_))
    document_->linksTruncated = true;
  trackedLink_ = false;
  inLink_ = false;
}

void BrowserHtmlParser::processTag() {
  if (tagLength_ < 3 || document_ == nullptr) return;
  if (tagLength_ >= 4 && tag_[1] == '!' && tag_[2] == '-' && tag_[3] == '-') {
    // Most comments fit in this bounded tag buffer. Do not leave the parser
    // stuck in comment mode when the closing marker is in the same chunk.
    inComment_ =
        !(tagLength_ >= 7 && tag_[tagLength_ - 3] == '-' && tag_[tagLength_ - 2] == '-' && tag_[tagLength_ - 1] == '>');
    return;
  }
  if (inComment_) {
    if (tagLength_ >= 4 && tag_[tagLength_ - 2] == '-' && tag_[tagLength_ - 3] == '-') inComment_ = false;
    return;
  }
  const char* name = nullptr;
  size_t nameLength = 0;
  tagName(tag_, tagLength_, name, nameLength);
  if (nameLength == 0) return;
  const bool closing = isClosingTag(tag_, tagLength_);
  if (inScript_ && !(closing && equalsIgnoreCase(name, nameLength, "script"))) {
    scriptEligible_ = false;
    return;
  }
  if (closing && equalsIgnoreCase(name, nameLength, "title")) inTitle_ = false;
  if (closing && equalsIgnoreCase(name, nameLength, "a")) endLink();
  if (closing && equalsIgnoreCase(name, nameLength, "script")) {
    inScript_ = false;
    skipText_ = false;
    runLiteralScript();
  }
  if (closing && (equalsIgnoreCase(name, nameLength, "style") || equalsIgnoreCase(name, nameLength, "noscript")))
    skipText_ = false;
  if (closing) {
    if (isBlockTag(name, nameLength)) emitBlockBreak();
    closeElement(name, nameLength);
    return;
  }
  if (!isVoidTag(name, nameLength) && tag_[tagLength_ - 2] != '/') pushElement(name, nameLength, tag_, tagLength_);
  if (failed_) return;
  if (isBlockTag(name, nameLength)) emitBlockBreak();
  if (equalsIgnoreCase(name, nameLength, "title")) inTitle_ = true;
  if (equalsIgnoreCase(name, nameLength, "script")) {
    const char* attributeValue = nullptr;
    size_t attributeLength = 0;
    inScript_ = true;
    scriptLength_ = 0;
    scriptCloseMatch_ = 0;
    scriptEligible_ = scriptCount_ < kMaxLiteralScripts &&
                      !findAttribute(tag_, tagLength_, "src", attributeValue, attributeLength) &&
                      !findAttribute(tag_, tagLength_, "type", attributeValue, attributeLength);
    if (scriptCount_ < kMaxLiteralScripts) ++scriptCount_;
    skipText_ = true;
  } else if (equalsIgnoreCase(name, nameLength, "style")) {
    const char* attributeValue = nullptr;
    size_t attributeLength = 0;
    inStyle_ = true;
    styleLength_ = 0;
    styleCloseMatch_ = 0;
    styleEligible_ = styleSheetCount_ < kMaxStyleSheets &&
                     (!findAttribute(tag_, tagLength_, "type", attributeValue, attributeLength) ||
                      equalsIgnoreCase(attributeValue, attributeLength, "text/css"));
    if (styleSheetCount_ < kMaxStyleSheets) ++styleSheetCount_;
    skipText_ = true;
  } else if (equalsIgnoreCase(name, nameLength, "noscript")) {
    skipText_ = true;
  }
  if (equalsIgnoreCase(name, nameLength, "a")) {
    const char* href = nullptr;
    size_t hrefLength = 0;
    inLink_ = findAttribute(tag_, tagLength_, "href", href, hrefLength);
    if (inLink_) beginLink(href, hrefLength);
  }
}

bool BrowserHtmlParser::feed(const uint8_t* data, const size_t length) {
  if (document_ == nullptr || data == nullptr || failed_) return false;
  for (size_t i = 0; i < length; ++i) {
    const char value = static_cast<char>(data[i]);
    if (inStyle_ && !inTag_) {
      constexpr char kClose[] = "</style>";
      if (styleCloseMatch_ != 0 || value == '<') {
        if (styleCloseMatch_ == sizeof(kClose) - 2 && std::isspace(static_cast<unsigned char>(value))) {
          // Whitespace before '>' is accepted by HTML.
        } else if (std::tolower(static_cast<unsigned char>(value)) == kClose[styleCloseMatch_]) {
          ++styleCloseMatch_;
          if (styleCloseMatch_ == sizeof(kClose) - 1) {
            inStyle_ = false;
            skipText_ = false;
            parseStyleSheet();
            closeElement("style", 5);
            styleCloseMatch_ = 0;
          }
        } else {
          // Unsupported '<' makes the whole sheet ineligible. Keep looking
          // for its close tag so CSS cannot leak into visible text.
          styleEligible_ = false;
          styleCloseMatch_ = value == '<' ? 1 : 0;
        }
      } else if (styleEligible_) {
        if (styleLength_ < kStyleBufferSize - 1)
          style_[styleLength_++] = value;
        else
          styleEligible_ = false;
      }
      continue;
    }
    if (inScript_ && !inTag_) {
      constexpr char kClose[] = "</script>";
      if (scriptCloseMatch_ != 0 || value == '<') {
        if (scriptCloseMatch_ == sizeof(kClose) - 2 && std::isspace(static_cast<unsigned char>(value))) {
          // HTML permits whitespace before the closing '>'.
        } else if (std::tolower(static_cast<unsigned char>(value)) == kClose[scriptCloseMatch_]) {
          ++scriptCloseMatch_;
          if (scriptCloseMatch_ == sizeof(kClose) - 1) {
            inScript_ = false;
            skipText_ = false;
            runLiteralScript();
            closeElement("script", 6);
            scriptCloseMatch_ = 0;
          }
        } else {
          // Any '<' other than the exact closing tag makes this script
          // unsupported; keep scanning for the closing tag without buffering.
          scriptEligible_ = false;
          scriptCloseMatch_ = value == '<' ? 1 : 0;
        }
      } else if (scriptEligible_) {
        if (scriptLength_ + 1 < kScriptBufferSize)
          script_[scriptLength_++] = value;
        else
          scriptEligible_ = false;
      }
      continue;
    }
    if (!inTag_) {
      if (inEntity_) {
        if (value == ';')
          processEntity();
        else if (!appendEntityChar(value)) {
          emitTextChar('&');
          emitText(entity_, entityLength_);
          entityLength_ = 0;
          inEntity_ = false;
          emitTextChar(value);
        }
      } else if (value == '&') {
        inEntity_ = true;
        entityLength_ = 0;
      } else if (value == '<') {
        inTag_ = true;
        tagLength_ = 0;
        quote_ = '\0';
        appendTagChar(value);
      } else {
        emitTextChar(value);
      }
      continue;
    }
    if (!appendTagChar(value)) return false;
    if (quote_ != '\0') {
      if (value == quote_) quote_ = '\0';
    } else if (value == '\'' || value == '"') {
      quote_ = value;
    } else if (value == '>') {
      processTag();
      inTag_ = false;
      tagLength_ = 0;
    }
  }
  return !failed_;
}

bool BrowserHtmlParser::finish() {
  if (inEntity_) processEntity();
  if (inLink_) endLink();
  return !failed_;
}

}  // namespace xpoint::browser
