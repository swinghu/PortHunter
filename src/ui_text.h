#pragma once

#include <raylib.h>

#include <vector>

namespace ui {

// Every on-screen string lives here so the font loader can derive the exact codepoint set.
namespace text {
extern const char *kAppTitle;
extern const char *kAppSubtitle;
extern const char *kPortLabel;
extern const char *kPortPlaceholder;
extern const char *kSearch;
extern const char *kRefresh;
extern const char *kQuickPorts;
extern const char *kHistory;
extern const char *kEmptyHistory;
extern const char *kPlatform;
extern const char *kPermissionHint;
extern const char *kSearching;
extern const char *kResultNone;
extern const char *kResultFound;
extern const char *kQueryFailed;
extern const char *kInvalidPort;
extern const char *kColPid;
extern const char *kColName;
extern const char *kColUser;
extern const char *kColProto;
extern const char *kColAddress;
extern const char *kCopyCommand;
extern const char *kCopied;
extern const char *kKill;
extern const char *kKillTitle;
extern const char *kKillQuestion;
extern const char *kKillWarn;
extern const char *kCancel;
extern const char *kConfirm;
extern const char *kKilled;
extern const char *kKillFailed;
extern const char *kFontMissing;
extern const char *kFooter;
}  // namespace text

const std::vector<const char *> &AllStrings();

// Loads a system CJK-capable font (or assets/fonts/ui.ttf when present); sets fellBackToDefault when unavailable.
Font LoadUiFont(int fontSize, bool &fellBackToDefault);

}  // namespace ui
