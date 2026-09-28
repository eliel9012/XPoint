#include "BrowserCore.h"

#include "network/HttpDownloader.h"

namespace xpoint::browser {

bool BrowserCore::fetch(const char* url) {
  if (url == nullptr || url[0] == '\0') return false;
  parser_.reset(document_, url);
  const bool fetched = HttpDownloader::fetchUrl(
      url, [this](const uint8_t* data, const size_t length) { return parser_.feed(data, length); });
  lastRequestSucceeded_ = fetched && parser_.finish();
  return lastRequestSucceeded_;
}

bool BrowserCore::open(const char* url) {
  if (!fetch(url)) return false;
  return history_.navigate(document_.url.c_str(), document_.title.c_str());
}

bool BrowserCore::goBack() {
  BrowserHistoryEntry destination;
  if (!history_.back(destination)) return false;
  return fetch(destination.url.c_str());
}

bool BrowserCore::goForward() {
  BrowserHistoryEntry destination;
  if (!history_.forward(destination)) return false;
  return fetch(destination.url.c_str());
}

}  // namespace xpoint::browser
