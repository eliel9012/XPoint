#pragma once

#include "BrowserHtmlParser.h"
#include "BrowserState.h"

namespace xpoint::browser {

/**
 * Synchronous MVP browser core. The caller owns rendering and input; this
 * class supplies a bounded document plus navigation primitives.
 */
class BrowserCore {
 public:
  bool open(const char* url);
  bool goBack();
  bool goForward();

  BrowserDocument& document() { return document_; }
  const BrowserDocument& document() const { return document_; }
  BrowserHistory& history() { return history_; }
  BrowserBookmarks& bookmarks() { return bookmarks_; }
  BrowserCache& cache() { return cache_; }
  const BrowserHistory& history() const { return history_; }
  const BrowserBookmarks& bookmarks() const { return bookmarks_; }
  const BrowserCache& cache() const { return cache_; }

  bool lastRequestSucceeded() const { return lastRequestSucceeded_; }

 private:
  bool fetch(const char* url);

  BrowserDocument document_;
  BrowserHtmlParser parser_;
  BrowserHistory history_;
  BrowserBookmarks bookmarks_;
  BrowserCache cache_;
  bool lastRequestSucceeded_ = false;
};

}  // namespace xpoint::browser
