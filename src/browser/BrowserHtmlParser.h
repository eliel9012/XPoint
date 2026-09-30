#pragma once

#include <cstddef>
#include <cstdint>

#include "BrowserTypes.h"

namespace xpoint::browser {

/**
 * Incremental, intentionally small HTML parser for e-ink pages.
 *
 * It keeps only the current tag and the output document. It extracts <title>,
 * visible text, and a bounded list of <a href="..."> links. A bounded CSS
 * subset affects text extraction; layout is ignored.
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
  static constexpr size_t kMaxElementDepth = 64;
  static constexpr size_t kScriptBufferSize = 256;
  static constexpr uint8_t kMaxLiteralScripts = 8;
  static constexpr size_t kStyleBufferSize = 512;
  static constexpr size_t kMaxStyleRules = 8;
  static constexpr uint8_t kMaxStyleSheets = 2;
  static constexpr size_t kSelectorSize = 32;

  enum class Transform : uint8_t { None, Uppercase, Lowercase };
  struct TextPresentation {
    Transform transform = Transform::None;
    bool preserveWhitespace = false;
    bool hidden = false;
  };
  struct ElementFrame {
    uint32_t nameHash = 0;
    TextPresentation presentation;
  };
  struct CssDeclarations {
    TextPresentation presentation;
    uint8_t properties = 0;
  };
  struct CssRule {
    char selector[kSelectorSize]{};
    uint8_t selectorLength = 0;
    uint8_t specificity = 0;
    CssDeclarations declarations;
  };

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
  void pushElement(const char* name, size_t nameLength, const char* tag, size_t tagLength);
  void closeElement(const char* name, size_t nameLength);
  void runLiteralScript();
  void parseStyleSheet();
  static void applyDeclarations(TextPresentation& target, const CssDeclarations& declarations);
  static CssDeclarations parseDeclarations(const char* style, size_t length);
  static bool matchesRule(const CssRule& rule, const char* name, size_t nameLength, const char* tag, size_t tagLength);

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
  ElementFrame elements_[kMaxElementDepth]{};
  size_t elementDepth_ = 0;
  size_t overflowDepth_ = 0;
  TextPresentation presentation_;
  char script_[kScriptBufferSize]{};
  size_t scriptLength_ = 0;
  uint8_t scriptCount_ = 0;
  bool inScript_ = false;
  bool scriptEligible_ = false;
  uint8_t scriptCloseMatch_ = 0;
  char style_[kStyleBufferSize]{};
  size_t styleLength_ = 0;
  uint8_t styleSheetCount_ = 0;
  bool inStyle_ = false;
  bool styleEligible_ = false;
  uint8_t styleCloseMatch_ = 0;
  CssRule rules_[kMaxStyleRules]{};
  size_t ruleCount_ = 0;
};

}  // namespace xpoint::browser
