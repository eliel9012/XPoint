#pragma once

#include <cstddef>
#include <cstdint>

#include "BrowserTypes.h"

namespace xpoint::browser {

/**
 * Incremental, intentionally small HTML parser for e-ink pages.
 *
 * It keeps only the current tag and the output document. It extracts <title>,
 * visible text, and a bounded list of <a href="..."> links. CSS, scripts,
 * forms, images, and layout are ignored by design.
 */
class BrowserHtmlParser {
 public:
  BrowserHtmlParser() = default;

  void reset(BrowserDocument& document, const char* pageUrl);
  bool feed(const uint8_t* data, size_t length);
  bool finish();
  bool failed() const { return failed_; }

 private:
  static constexpr size_t kTagBufferSize = 256;
  static constexpr size_t kEntityBufferSize = 16;

  void processTag();
  void processEntity();
  void emitTextChar(char value);
  void emitText(const char* value, size_t length);
  void emitBlockBreak();
  void emitLinkText(char value);
  void beginLink(const char* href, size_t length);
  void endLink();
  bool appendTagChar(char value);
  bool appendEntityChar(char value);

  BrowserDocument* document_ = nullptr;
  char tag_[kTagBufferSize]{};
  size_t tagLength_ = 0;
  bool inTag_ = false;
  char quote_ = '\0';
  bool inComment_ = false;
  bool skipText_ = false;
  bool inTitle_ = false;
  bool inLink_ = false;
  bool trackedLink_ = false;
  bool pendingSpace_ = false;
  bool titlePendingSpace_ = false;
  bool failed_ = false;
  char entity_[kEntityBufferSize]{};
  size_t entityLength_ = 0;
  bool inEntity_ = false;
  BrowserLink currentLink_;
};

}  // namespace xpoint::browser
