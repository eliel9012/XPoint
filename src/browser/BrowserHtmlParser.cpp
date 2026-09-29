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
  if (length == 0 || (length >= 1 && value[0] == '#')) return false;
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
  const char* style = nullptr;
  size_t styleLength = 0;
  if (findAttribute(tag, tagLength, "style", style, styleLength)) {
    // Only the bounded tag buffer is scanned. No URLs, selectors, variables,
    // expressions, fonts, colors, or layout properties are interpreted.
    size_t cursor = 0;
    while (cursor < styleLength) {
      const size_t start = cursor;
      while (cursor < styleLength && style[cursor] != ';') ++cursor;
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
      if (equalsIgnoreCase(property, propertyLength, "display") && equalsIgnoreCase(value, valueLength, "none"))
        next.hidden = true;
      else if (equalsIgnoreCase(property, propertyLength, "white-space")) {
        if (equalsIgnoreCase(value, valueLength, "pre"))
          next.preserveWhitespace = true;
        else if (equalsIgnoreCase(value, valueLength, "normal"))
          next.preserveWhitespace = false;
      } else if (equalsIgnoreCase(property, propertyLength, "text-transform")) {
        if (equalsIgnoreCase(value, valueLength, "uppercase"))
          next.transform = Transform::Uppercase;
        else if (equalsIgnoreCase(value, valueLength, "lowercase"))
          next.transform = Transform::Lowercase;
        else if (equalsIgnoreCase(value, valueLength, "none"))
          next.transform = Transform::None;
      }
    }
  }
  elements_[elementDepth_++] = {nameHash(name, nameLength), next};
  presentation_ = next;
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
  } else if (equalsIgnoreCase(name, nameLength, "style") || equalsIgnoreCase(name, nameLength, "noscript")) {
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
  if (document_ != nullptr && document_->text.size() != 0 && document_->text[document_->text.size() - 1] == '\n') {
    while (document_->text.size() > 0 && document_->text[document_->text.size() - 1] == '\n') {
      // FixedString has no resize; one trailing newline is intentionally kept.
      break;
    }
  }
  return !failed_;
}

}  // namespace xpoint::browser
