#include "ui_text.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ui {
namespace text {

const char *kAppTitle = "端口进程查找";
const char *kAppSubtitle = "输入被占用的 HTTP 端口，定位占用进程";
const char *kPortLabel = "端口号";
const char *kPortPlaceholder = "例如 8080";
const char *kSearch = "查找";
const char *kRefresh = "刷新";
const char *kQuickPorts = "常用端口";
const char *kHistory = "查询历史";
const char *kEmptyHistory = "暂无记录";
const char *kPlatform = "当前平台";
const char *kPermissionHint = "结束其它用户的进程需要管理员/root 权限";
const char *kSearching = "查询中…";
const char *kResultNone = "端口 %d 没有被任何进程监听";
const char *kResultFound = "端口 %d 被 %d 个进程占用";
const char *kQueryFailed = "查询失败：%s";
const char *kInvalidPort = "请输入 1 - 65535 之间的端口号";
const char *kColPid = "PID";
const char *kColName = "进程名";
const char *kColUser = "用户";
const char *kColProto = "协议";
const char *kColAddress = "监听地址";
const char *kCopyCommand = "复制命令";
const char *kCopied = "已复制：%s";
const char *kKill = "结束";
const char *kKillTitle = "确认结束进程";
const char *kKillQuestion = "将执行以下命令，PID %d（%s）会立即退出：";
const char *kKillWarn = "未保存的数据会丢失，确认继续？";
const char *kCancel = "取消";
const char *kConfirm = "确认执行";
const char *kKilled = "已结束进程 PID %d";
const char *kKillFailed = "结束失败：%s";
const char *kFontMissing = "未找到中文字体，界面可能显示异常；可在 assets/fonts/ui.ttf 放置字体文件";
const char *kFooter = "回车查询 · Esc 关闭确认框";

}  // namespace text

const std::vector<const char *> &AllStrings() {
    static const std::vector<const char *> kAll = {
        text::kAppTitle,       text::kAppSubtitle,   text::kPortLabel,       text::kPortPlaceholder,
        text::kSearch,         text::kRefresh,       text::kQuickPorts,      text::kHistory,
        text::kEmptyHistory,   text::kPlatform,      text::kPermissionHint,  text::kSearching,
        text::kResultNone,     text::kResultFound,   text::kQueryFailed,     text::kInvalidPort,
        text::kColPid,         text::kColName,       text::kColUser,         text::kColProto,
        text::kColAddress,     text::kCopyCommand,   text::kCopied,          text::kKill,
        text::kKillTitle,      text::kKillQuestion,  text::kKillWarn,        text::kCancel,
        text::kConfirm,        text::kKilled,        text::kKillFailed,      text::kFontMissing,
        text::kFooter,
    };
    return kAll;
}

namespace {

std::vector<int> BuildCodepoints() {
    std::vector<int> codepoints;
    for (int ascii = 32; ascii <= 126; ++ascii) codepoints.push_back(ascii);

    for (const char *label : AllStrings()) {
        const auto *bytes = reinterpret_cast<const unsigned char *>(label);
        while (*bytes) {
            int codepoint = *bytes;
            int extra = 0;
            if ((*bytes & 0xE0) == 0xC0) {
                codepoint = *bytes & 0x1F;
                extra = 1;
            } else if ((*bytes & 0xF0) == 0xE0) {
                codepoint = *bytes & 0x0F;
                extra = 2;
            } else if ((*bytes & 0xF8) == 0xF0) {
                codepoint = *bytes & 0x07;
                extra = 3;
            }
            ++bytes;
            for (int i = 0; i < extra && *bytes; ++i, ++bytes) codepoint = (codepoint << 6) | (*bytes & 0x3F);
            if (codepoint > 0x1F && std::find(codepoints.begin(), codepoints.end(), codepoint) == codepoints.end()) {
                codepoints.push_back(codepoint);
            }
        }
    }
    return codepoints;
}

const char *const *FontCandidates() {
    static const char *kCandidates[] = {
        "assets/fonts/ui.ttf",
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyh.ttf",
        "C:\\Windows\\Fonts\\simhei.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
#else
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/arphic/uming.ttc",
#endif
        nullptr,
    };
    return kCandidates;
}

bool FileExists(const char *path) {
    FILE *handle = std::fopen(path, "rb");
    if (!handle) return false;
    std::fclose(handle);
    return true;
}

}  // namespace

Font LoadUiFont(int fontSize, bool &fellBackToDefault) {
    // raylib takes a mutable codepoint array, so this vector must not be const.
    auto codepoints = BuildCodepoints();
    for (const char *const *candidate = FontCandidates(); *candidate; ++candidate) {
        if (!FileExists(*candidate)) continue;
        Font font = LoadFontEx(*candidate, fontSize, codepoints.data(), static_cast<int>(codepoints.size()));
        if (font.glyphCount > 0 && font.texture.id != 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            fellBackToDefault = false;
            return font;
        }
    }
    fellBackToDefault = true;
    return GetFontDefault();
}

}  // namespace ui
