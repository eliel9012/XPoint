#include "ReaderPreferences.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "CrossPointSettings.h"

namespace ReaderPreferences {
namespace {

void copySource(char* destination, const size_t capacity, const char* source) {
  if (capacity == 0) return;
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}  // namespace

Snapshot snapshot() {
  Snapshot result;
#if defined(CROSSPOINT_TTF_READER)
  if (SETTINGS.readerFontEngine == CrossPointSettings::READER_ENGINE_TTF) {
    result.fontSize = SETTINGS.ttfFontPointSize;
    copySource(result.source, sizeof(result.source),
               SETTINGS.ttfFontFamilyName[0] != '\0' ? SETTINGS.ttfFontFamilyName : tr(STR_BUILTIN_FONT));
  } else
#endif
  {
    result.fontSize = SETTINGS.fontPointSize;
    if (SETTINGS.sdFontFamilyName[0] != '\0') {
      copySource(result.source, sizeof(result.source), SETTINGS.sdFontFamilyName);
    } else {
      static constexpr StrId BUILTIN_FONTS[] = {StrId::STR_NOTO_SERIF, StrId::STR_ATKINSON_HN,
                                                StrId::STR_ATKINSON_HN};
      const auto index = std::min<uint8_t>(SETTINGS.fontFamily, static_cast<uint8_t>(std::size(BUILTIN_FONTS) - 1));
      copySource(result.source, sizeof(result.source), I18N.get(BUILTIN_FONTS[index]));
    }
  }

  const auto footer = SETTINGS.statusBarSpec();
  result.showsPageCount = footer.showChapterPageCount;
  result.showsTimeLeft = footer.showChapterTimeLeft;
  result.showsProgress = footer.showBookProgressPercent;
  result.showsTitle = footer.showsTitle();
  result.showsBattery = footer.showBattery;
  result.showsProgressBar = footer.showsProgressBar();
  return result;
}

void formatNotificationDetail(char* buffer, const size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) return;
  const Snapshot current = snapshot();
  char footer[48] = {};
  size_t used = 0;
  const auto append = [&footer, &used](const char* text) {
    if (used != 0 && used + 1 < sizeof(footer)) footer[used++] = '/';
    if (used >= sizeof(footer) - 1) return;
    const size_t available = sizeof(footer) - used - 1;
    const size_t length = std::min(strlen(text), available);
    memcpy(footer + used, text, length);
    used += length;
    footer[used] = '\0';
  };
  if (current.showsPageCount) append(tr(STR_CHAPTER_PAGE_COUNT));
  if (current.showsTimeLeft) append(tr(STR_TIME_LEFT));
  if (current.showsProgress) append(tr(STR_BOOK_PROGRESS_PERCENTAGE));
  if (current.showsTitle) append(tr(STR_TITLE));
  if (current.showsBattery) append(tr(STR_BATTERY));
  if (current.showsProgressBar) append(tr(STR_PROGRESS_BAR));
  if (used == 0) copySource(footer, sizeof(footer), tr(STR_HIDE));

  snprintf(buffer, bufferSize, "%s: %s | %s: %u pt | %s: %s", tr(STR_READER_SOURCE), current.source,
           tr(STR_FONT_SIZE), static_cast<unsigned>(current.fontSize), tr(STR_READING_FOOTER), footer);
  buffer[bufferSize - 1] = '\0';
}

}  // namespace ReaderPreferences
