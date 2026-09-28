#include "BrowserHtmlParser.h"

#include <cctype>
#include <cstring>

namespace xpoint::browser {
namespace {

bool equalsIgnoreCase(const char* value, size_t length, const char* expected) {
  const size_t expectedLength = std::strlen(expected);
  if (length != expectedLength) return false;
  for (size_t i = 0; i < length; ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) !=
        std::tolower(static_cast<unsigned char>(expected[i])))
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

bool isClosingTag(const char* tag, size_t length) {
  return length > 1 && tag[0] == '<' && tag[1] == '/';
}

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
  if (std::isspace(static_cast<unsigned char>(value))) {
    pendingSpace_ = !document_->text.empty();
    return;
  }
  if (pendingSpace_ && !document_->text.empty() && document_->text[document_->text.size() - 1] != '\n') {
    if (!document_->text.append(' ')) document_->textTruncated = true;
  }
  pendingSpace_ = false;
  if (!document_->text.append(value)) document_->textTruncated = true;
  emitLinkText(value);
}

void BrowserHtmlParser::emitText(const char* value, const size_t length) {
  for (size_t i = 0; i < length; ++i) emitTextChar(value[i]);
}

void BrowserHtmlParser::emitBlockBreak() {
  if (document_ == nullptr || document_->text.empty()) return;
  pendingSpace_ = false;
  if (document_->text[document_->text.size() - 1] != '\n' && !document_->text.append('\n')) document_->textTruncated = true;
}

void BrowserHtmlParser::processEntity() {
  if (!inEntity_) return;
  if (equalsIgnoreCase(entity_, entityLength_, "amp")) emitTextChar('&');
  else if (equalsIgnoreCase(entity_, entityLength_, "lt")) emitTextChar('<');
  else if (equalsIgnoreCase(entity_, entityLength_, "gt")) emitTextChar('>');
  else if (equalsIgnoreCase(entity_, entityLength_, "quot")) emitTextChar('"');
  else if (equalsIgnoreCase(entity_, entityLength_, "apos") || equalsIgnoreCase(entity_, entityLength_, "#39")) emitTextChar('\'');
  else if (equalsIgnoreCase(entity_, entityLength_, "nbsp")) emitTextChar(' ');
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
  if (document_ == nullptr) return;
  if (document_->links.full()) {
    document_->linksTruncated = true;
    return;
  }
  if (!resolveUrl(document_->url.c_str(), document_->url.size(), href, length, currentLink_.url)) return;
  trackedLink_ = true;
}

void BrowserHtmlParser::endLink() {
  if (trackedLink_ && document_ != nullptr && !document_->links.push_back(currentLink_)) document_->linksTruncated = true;
  trackedLink_ = false;
  inLink_ = false;
}

void BrowserHtmlParser::processTag() {
  if (tagLength_ < 3 || document_ == nullptr) return;
  if (tagLength_ >= 4 && tag_[1] == '!' && tag_[2] == '-' && tag_[3] == '-') {
    // Most comments fit in this bounded tag buffer. Do not leave the parser
    // stuck in comment mode when the closing marker is in the same chunk.
    inComment_ = !(tagLength_ >= 7 && tag_[tagLength_ - 3] == '-' && tag_[tagLength_ - 2] == '-' &&
                   tag_[tagLength_ - 1] == '>');
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
  if (closing && equalsIgnoreCase(name, nameLength, "title")) inTitle_ = false;
  if (closing && equalsIgnoreCase(name, nameLength, "a")) endLink();
  if (closing && (equalsIgnoreCase(name, nameLength, "script") || equalsIgnoreCase(name, nameLength, "style") ||
                  equalsIgnoreCase(name, nameLength, "noscript"))) skipText_ = false;
  if (isBlockTag(name, nameLength)) emitBlockBreak();
  if (closing) return;
  if (equalsIgnoreCase(name, nameLength, "title")) inTitle_ = true;
  if (equalsIgnoreCase(name, nameLength, "script") || equalsIgnoreCase(name, nameLength, "style") ||
      equalsIgnoreCase(name, nameLength, "noscript")) skipText_ = true;
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
    if (!inTag_) {
      if (inEntity_) {
        if (value == ';') processEntity();
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
