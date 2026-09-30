#include "SleepWallpaper.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "CrossPointState.h"

namespace {

constexpr char ROOT_SLEEP_BITMAP[] = "/sleep.bmp";
constexpr char HIDDEN_SLEEP_DIRECTORY[] = "/.sleep";
constexpr char LEGACY_SLEEP_DIRECTORY[] = "/sleep";
constexpr size_t MAX_ASSET_NAME_LENGTH = 256;

bool isRecent(const uint16_t index, const uint8_t window) { return APP_STATE.isRecentSleep(index, window); }

bool isValidBmp(HalFile& file) {
  Bitmap bitmap(file);
  return bitmap.parseHeaders() == BmpReaderError::Ok;
}

bool nextValidBmp(HalFile& directory, char* name) {
  for (auto file = directory.openNextFile(); file; file = directory.openNextFile()) {
    if (file.isDirectory()) continue;

    file.getName(name, MAX_ASSET_NAME_LENGTH);
    if (name[0] == '\0' || name[0] == '.' || !FsHelpers::hasBmpExtension(std::string_view{name})) continue;
    if (isValidBmp(file)) return true;
  }
  return false;
}

bool selectFromDirectory(const char* directoryPath, std::string& output, const bool commitRecent) {
  auto directory = Storage.open(directoryPath);
  if (!directory || !directory.isDirectory()) return false;

  auto name = makeUniqueNoThrow<char[]>(MAX_ASSET_NAME_LENGTH);
  if (!name) {
    LOG_ERR("SLP", "OOM: sleep wallpaper filename buffer");
    return false;
  }

  uint16_t fileCount = 0;
  while (fileCount < UINT16_MAX && nextValidBmp(directory, name.get())) ++fileCount;
  if (fileCount == 0) return false;

  const uint8_t recentWindow =
      static_cast<uint8_t>(std::min<uint16_t>(APP_STATE.recentSleepFill, static_cast<uint16_t>(fileCount - 1)));
  uint16_t selectedIndex = static_cast<uint16_t>(random(fileCount));
  for (uint8_t attempt = 0; attempt < 20 && isRecent(selectedIndex, recentWindow); ++attempt) {
    selectedIndex = static_cast<uint16_t>(random(fileCount));
  }

  directory.rewindDirectory();
  for (uint16_t index = 0; index <= selectedIndex; ++index) {
    if (!nextValidBmp(directory, name.get())) return false;
  }

  output.assign(directoryPath);
  output.push_back('/');
  output.append(name.get());

  if (commitRecent) {
    APP_STATE.pushRecentSleep(selectedIndex);
    APP_STATE.saveToFile();
  }
  return true;
}

bool selectRootBitmap(std::string& output) {
  HalFile file;
  if (!Storage.openFileForRead("SLP", ROOT_SLEEP_BITMAP, file) || !isValidBmp(file)) return false;
  output = ROOT_SLEEP_BITMAP;
  return true;
}

bool selectCustomBitmap(std::string& output, const bool commitRecent) {
  if (selectRootBitmap(output)) return true;
  if (selectFromDirectory(HIDDEN_SLEEP_DIRECTORY, output, commitRecent)) return true;
  return selectFromDirectory(LEGACY_SLEEP_DIRECTORY, output, commitRecent);
}

}  // namespace

SleepWallpaper::Selection SleepWallpaper::select(const Fallback& fallback, const bool commitRecent) {
  Selection selection;
  if (selectCustomBitmap(selection.path, commitRecent)) {
    selection.kind = Kind::Asset;
    return selection;
  }

  if (fallback.coverBmpPath != nullptr && fallback.coverBmpPath[0] != '\0') {
    HalFile file;
    if (Storage.openFileForRead("SLP", fallback.coverBmpPath, file) && isValidBmp(file)) {
      selection.kind = Kind::LastBookCover;
      selection.path = fallback.coverBmpPath;
      return selection;
    }
  }

  if (fallback.title != nullptr && fallback.title[0] != '\0') {
    selection.kind = Kind::LastBookTitle;
    selection.title = fallback.title;
  }
  return selection;
}
