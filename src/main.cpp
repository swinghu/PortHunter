#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <vector>

#include "port_scanner.h"
#include "ui_text.h"
#include "ui_widgets.h"

namespace {

constexpr int kWindowWidth = 1180;
constexpr int kWindowHeight = 700;
constexpr float kLeftWidth = 320;
constexpr float kContentX = kLeftWidth + 24;
constexpr int kFontTitle = 21;
constexpr int kFontHeading = 17;
constexpr int kFontBody = 15;
constexpr int kFontSmall = 12;
constexpr float kRowHeight = 60;
constexpr float kTableTop = 120;
constexpr const int kQuickPortValues[] = {3000, 5173, 8080, 8888};

using namespace ui;
using namespace ui::text;

struct App {
    Font font;
    bool fontFallback = false;
    std::string portInput;
    int activePort = 0;
    std::vector<ProcessInfo> rows;
    std::vector<int> history;
    std::future<ScanResult> inFlight;
    bool inputFocused = true;
    bool confirmVisible = false;
    size_t confirmRow = 0;
    std::string notice;
    float scroll = 0;
};

bool ParsePort(const std::string &value, uint16_t &port) {
    if (value.empty() || value.size() > 5) return false;
    int number = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '9') return false;
        number = number * 10 + (digit - '0');
    }
    if (number < 1 || number > 65535) return false;
    port = static_cast<uint16_t>(number);
    return true;
}

void LaunchScan(App &app) {
    uint16_t port = 0;
    if (!ParsePort(app.portInput, port)) {
        app.notice = kInvalidPort;
        return;
    }
    if (app.inFlight.valid()) return;
    app.activePort = port;
    app.notice.clear();
    app.inFlight = std::async(std::launch::async, ScanPort, port);
}

void FinishScan(App &app) {
    if (!app.inFlight.valid()) return;
    if (app.inFlight.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    const ScanResult result = app.inFlight.get();
    if (!result.ok) {
        app.rows.clear();
        app.notice = Format(kQueryFailed, result.error.c_str());
        return;
    }
    app.rows = result.processes;
    app.scroll = 0;
    app.history.erase(std::remove(app.history.begin(), app.history.end(), result.port), app.history.end());
    app.history.insert(app.history.begin(), result.port);
    if (app.history.size() > 10) app.history.resize(10);
}

bool Scanning(const App &app) {
    return app.inFlight.valid() && app.inFlight.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void HandleKeys(App &app) {
    if (app.confirmVisible) {
        if (IsKeyPressed(KEY_ESCAPE)) app.confirmVisible = false;
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        LaunchScan(app);
        return;
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !app.portInput.empty()) app.portInput.pop_back();
    int pressed = 0;
    while ((pressed = GetCharPressed()) != 0) {
        if (pressed >= '0' && pressed <= '9' && app.portInput.size() < 5) {
            app.portInput.push_back(static_cast<char>(pressed));
        }
    }
}

void DrawLeftPanel(App &app) {
    const auto &theme = kPalette;
    DrawRectangle(0, 0, static_cast<int>(kLeftWidth), kWindowHeight, theme.leftPanel);
    DrawLine(static_cast<int>(kLeftWidth), 0, static_cast<int>(kLeftWidth), kWindowHeight, theme.divider);

    const float x = 24;
    DrawLabel(kAppTitle, x, 26, kFontTitle, app.font, theme.text);
    DrawLabel(kAppSubtitle, x, 56, kFontSmall, app.font, theme.muted);

    DrawLabel(kPortLabel, x, 104, kFontSmall, app.font, theme.muted);
    const Rect input{x, 124, 196, 38};
    const auto mouse = GetMousePosition();
    if (Contains(input, mouse) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) app.inputFocused = true;
    DrawRectangleRec(input, theme.field);
    DrawRectangleLinesEx(input, app.inputFocused ? 1.5f : 1.0f,
                         app.inputFocused ? theme.accent : theme.fieldBorder);
    const auto display = app.portInput.empty() ? kPortPlaceholder : app.portInput;
    DrawLabel(display.c_str(), input.x + 12, input.y + 11, kFontBody, app.font,
              app.portInput.empty() ? theme.muted : theme.text);
    if (app.inputFocused && static_cast<int>(GetTime() * 2) % 2 == 0) {
        const auto width = MeasureTextEx(app.font, app.portInput.c_str(), kFontBody, 1.0f).x;
        DrawRectangleRec(Rect{input.x + 12 + width + 2, input.y + 9, 1.5f, kFontBody}, theme.accent);
    }

    const Rect search{x + 208, 124, 64, 38};
    if (Button(search, kSearch, app.font, kFontBody, true)) LaunchScan(app);
    DrawLabel(kFooter, x, 172, kFontSmall, app.font, theme.muted);

    DrawLabel(kQuickPorts, x, 214, kFontSmall, app.font, theme.muted);
    for (int index = 0; index < 4; ++index) {
        const Rect chip{x + index * 68.0f, 234, 62, 30};
        const std::string chipLabel = std::to_string(kQuickPortValues[index]);
        if (Button(chip, chipLabel.c_str(), app.font, kFontSmall)) {
            app.portInput = chipLabel;
            LaunchScan(app);
        }
    }

    DrawLabel(kHistory, x, 292, kFontSmall, app.font, theme.muted);
    if (app.history.empty()) {
        DrawLabel(kEmptyHistory, x, 320, kFontBody, app.font, theme.muted);
    }
    for (size_t index = 0; index < app.history.size() && index < 8; ++index) {
        const Rect entry{x, 314.0f + static_cast<float>(index) * 38.0f, 252, 32};
        const auto hovered = Contains(entry, mouse);
        if (hovered) DrawRectangleRec(entry, theme.rowHover);
        const int port = app.history[index];
        const bool active = app.activePort == port;
        DrawLabel(std::to_string(port).c_str(), entry.x + 12, entry.y + 8, kFontBody, app.font,
                  active ? theme.accent : theme.text);
        if (active) DrawRectangleRec(Rect{entry.x, entry.y + 6, 3, 20}, theme.accent);
        if (hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            app.portInput = std::to_string(port);
            LaunchScan(app);
        }
    }

    DrawRectangle(0, kWindowHeight - 96, static_cast<int>(kLeftWidth), 96, Fade(theme.field, 0.4f));
    DrawLabel(Format("%s: %s", kPlatform, PlatformName().c_str()).c_str(), x, kWindowHeight - 78, kFontSmall,
              app.font, theme.muted);
    DrawLabel(kPermissionHint, x, kWindowHeight - 54, kFontSmall, app.font, theme.muted);
}

void DrawResultTable(App &app) {
    const auto &theme = kPalette;
    const float headerTop = 96;
    DrawLabel(kColPid, kContentX, headerTop, kFontSmall, app.font, theme.muted);
    DrawLabel(kColName, kContentX + 62, headerTop, kFontSmall, app.font, theme.muted);
    DrawLabel(kColUser, kContentX + 232, headerTop, kFontSmall, app.font, theme.muted);
    DrawLabel(kColProto, kContentX + 342, headerTop, kFontSmall, app.font, theme.muted);
    DrawLabel(kColAddress, kContentX + 402, headerTop, kFontSmall, app.font, theme.muted);
    DrawLine(static_cast<int>(kContentX), 116, kWindowWidth - 24, 116, theme.divider);

    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        const auto maxScroll = std::max(0.0f, static_cast<float>(app.rows.size()) * kRowHeight - 420.0f);
        app.scroll = std::min(maxScroll, std::max(0.0f, app.scroll - wheel * 30.0f));
    }

    for (size_t index = 0; index < app.rows.size(); ++index) {
        const ProcessInfo &info = app.rows[index];
        const float y = kTableTop + index * kRowHeight - app.scroll;
        if (y + kRowHeight < kTableTop || y > kWindowHeight - 40) continue;

        const Rect band{kContentX - 8, y - 6, kWindowWidth - kContentX - 16, kRowHeight - 4};
        if (Contains(band, GetMousePosition())) DrawRectangleRec(band, Fade(theme.rowHover, 0.6f));

        DrawLabel(std::to_string(info.pid).c_str(), kContentX, y + 4, kFontBody, app.font, theme.text);
        DrawLabel(Elide(app.font, kFontBody, info.name, 160).c_str(), kContentX + 62, y + 4, kFontBody, app.font,
                  theme.text);
        DrawLabel(Elide(app.font, kFontBody, info.user, 100).c_str(), kContentX + 232, y + 4, kFontBody, app.font,
                  theme.text);
        DrawLabel(info.proto.c_str(), kContentX + 342, y + 4, kFontBody, app.font, theme.text);
        DrawLabel(Elide(app.font, kFontBody, info.address, 180).c_str(), kContentX + 402, y + 4, kFontBody, app.font,
                  theme.text);
        DrawLabel(Elide(app.font, kFontSmall, info.exePath.empty() ? "-" : info.exePath, 560).c_str(), kContentX + 4,
                  y + 26, kFontSmall, app.font, theme.muted);

        const Rect copy{kContentX + 600, y + 2, 104, 28};
        if (Button(copy, kCopyCommand, app.font, kFontSmall)) {
            SetClipboardText(info.killCommand.c_str());
            app.notice = Format(kCopied, info.killCommand.c_str());
        }
        const Rect kill{kContentX + 712, y + 2, 76, 28};
        if (Button(kill, kKill, app.font, kFontSmall, false, true)) {
            app.confirmRow = index;
            app.confirmVisible = true;
        }
        DrawLine(static_cast<int>(kContentX), static_cast<int>(y + kRowHeight - 8), kWindowWidth - 24,
                 static_cast<int>(y + kRowHeight - 8), Fade(theme.divider, 0.5f));
    }
}

void DrawRightPanel(App &app) {
    const auto &theme = kPalette;
    std::string heading;
    if (Scanning(app)) {
        heading = kSearching;
    } else if (app.activePort == 0) {
        heading = kAppSubtitle;
    } else if (app.rows.empty()) {
        heading = Format(kResultNone, app.activePort);
    } else {
        heading = Format(kResultFound, app.activePort, static_cast<int>(app.rows.size()));
    }
    DrawLabel(heading.c_str(), kContentX, 34, kFontHeading, app.font, theme.text);

    const Rect refresh{kWindowWidth - 24 - 92, 28, 92, 32};
    if (Button(refresh, kRefresh, app.font, kFontSmall)) LaunchScan(app);

    if (!app.rows.empty()) DrawResultTable(app);
}

void DrawConfirmDialog(App &app) {
    if (app.confirmRow >= app.rows.size()) app.confirmVisible = false;
    if (!app.confirmVisible) return;

    const ProcessInfo &info = app.rows[app.confirmRow];
    const auto &theme = kPalette;
    DrawRectangle(0, 0, kWindowWidth, kWindowHeight, theme.overlay);

    const Rect box{(kWindowWidth - 560) / 2.0f, (kWindowHeight - 236) / 2.0f, 560, 236};
    DrawRectangleRec(box, theme.leftPanel);
    DrawRectangleLinesEx(box, 1.0f, theme.fieldBorder);

    DrawLabel(kKillTitle, box.x + 24, box.y + 22, kFontHeading, app.font, theme.text);
    DrawLabel(Format(kKillQuestion, info.pid, info.name.c_str()).c_str(), box.x + 24, box.y + 56, kFontBody, app.font,
              theme.muted);

    const Rect commandField{box.x + 24, box.y + 88, box.width - 48, 40};
    DrawRectangleRec(commandField, theme.field);
    DrawRectangleLinesEx(commandField, 1.0f, theme.accent);
    DrawLabel(info.killCommand.c_str(), commandField.x + 14, commandField.y + 12, kFontBody, app.font, theme.text);

    DrawLabel(kKillWarn, box.x + 24, box.y + 144, kFontSmall, app.font, theme.muted);

    const Rect confirm{box.x + box.width - 24 - 108, box.y + box.height - 24 - 34, 108, 34};
    const Rect cancel{confirm.x - 92, confirm.y, 80, 34};
    if (Button(cancel, kCancel, app.font, kFontBody)) {
        app.confirmVisible = false;
        return;
    }
    if (Button(confirm, kConfirm, app.font, kFontBody, false, true)) {
        app.confirmVisible = false;
        std::string error;
        if (TerminateProcess(info, error)) {
            app.notice = Format(kKilled, info.pid);
        } else {
            app.notice = Format(kKillFailed, error.c_str());
        }
        LaunchScan(app);
    }
}

void DrawNotice(App &app) {
    if (app.notice.empty()) return;
    DrawRectangle(0, kWindowHeight - 28, kWindowWidth, 28, Fade(kPalette.field, 0.2f));
    DrawLabel(app.notice.c_str(), kContentX, kWindowHeight - 20, kFontSmall, app.font, kPalette.text);
}

}  // namespace

int main(int argc, char *argv[]) {
    InitWindow(kWindowWidth, kWindowHeight, kAppTitle);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    App app;
    app.font = LoadUiFont(kFontBody, app.fontFallback);

#ifndef __APPLE__
    // macOS has no window icon (GLFW warns); the .icns in the bundle covers the Dock and Finder.
    Image icon = {0};
    if (FileExists("assets/icon.png")) icon = LoadImage("assets/icon.png");
    if (icon.data != nullptr) SetWindowIcon(icon);  // kept loaded: GLFW may reference the pixels
#endif
    if (argc > 1) {
        app.portInput = argv[1];
        LaunchScan(app);
    }

    while (!WindowShouldClose()) {
        HandleKeys(app);
        FinishScan(app);

        BeginDrawing();
        ClearBackground(kPalette.window);
        DrawLeftPanel(app);
        DrawRightPanel(app);
        DrawNotice(app);
        DrawConfirmDialog(app);
        if (app.fontFallback) DrawLabel(kFontMissing, kContentX, kWindowHeight - 46, kFontSmall, app.font, kPalette.danger);
        EndDrawing();
    }

    UnloadFont(app.font);
    CloseWindow();
    return 0;
}
