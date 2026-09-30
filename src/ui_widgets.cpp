#include "ui_widgets.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ui {

const Palette kPalette = {
    /*window       */{0x1a, 0x1c, 0x21, 0xff},
    /*leftPanel    */{0x23, 0x26, 0x2d, 0xff},
    /*rightPanel   */{0x1a, 0x1c, 0x21, 0xff},
    /*field        */{0x14, 0x16, 0x1b, 0xff},
    /*fieldBorder  */{0x3a, 0x40, 0x4d, 0xff},
    /*accent       */{0x4a, 0x7d, 0xff, 0xff},
    /*accentPressed*/{0x35, 0x5f, 0xd6, 0xff},
    /*danger       */{0xd9, 0x53, 0x3f, 0xff},
    /*text         */{0xe8, 0xea, 0xef, 0xff},
    /*muted        */{0x8f, 0x97, 0xa6, 0xff},
    /*divider      */{0x33, 0x38, 0x42, 0xff},
    /*rowHover     */{0x2a, 0x2e, 0x37, 0xff},
    /*overlay      */{0x00, 0x00, 0x00, 0xb0},
};

bool Contains(const Rect &rect, Vector2 point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

bool Button(const Rect &rect, const char *label, const Font &font, int fontSize, bool primary, bool danger,
            bool enabled) {
    const auto mouse = GetMousePosition();
    const auto hovered = enabled && Contains(rect, mouse);
    const auto pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const auto labelSize = MeasureTextEx(font, label, static_cast<float>(fontSize), 1.0f);

    Color background = primary ? (pressed ? kPalette.accentPressed : kPalette.accent)
                               : danger ? (pressed ? Fade(kPalette.danger, 0.7f) : Fade(kPalette.danger, 0.85f))
                                        : (pressed ? kPalette.field : kPalette.rowHover);
    if (!enabled) background = Fade(kPalette.rowHover, 0.5f);

    DrawRectangleRec(rect, background);
    DrawRectangleLinesEx(rect, 1.0f, hovered || primary ? kPalette.accent : kPalette.fieldBorder);
    const auto textColor = enabled ? kPalette.text : kPalette.muted;
    const auto width = MeasureTextEx(font, label, static_cast<float>(fontSize), 1.0f).x;
    DrawTextEx(font, label, {rect.x + (rect.width - width) / 2.0f, rect.y + (rect.height - fontSize) / 2.0f},
               static_cast<float>(fontSize), 1.0f, textColor);
    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

void DrawLabel(const char *label, float x, float y, int fontSize, const Font &font, Color color) {
    DrawTextEx(font, label, {x, y}, static_cast<float>(fontSize), 1.0f, color);
}

std::string Elide(const Font &font, int fontSize, const std::string &value, float maxWidth) {
    if (MeasureTextEx(font, value.c_str(), static_cast<float>(fontSize), 1.0f).x <= maxWidth) return value;
    std::string clipped;
    size_t pos = 0;
    while (pos < value.size()) {
        const unsigned char lead = static_cast<unsigned char>(value[pos]);
        size_t length = 1;
        if ((lead & 0xE0) == 0xC0) length = 2;
        else if ((lead & 0xF0) == 0xE0) length = 3;
        else if ((lead & 0xF8) == 0xF0) length = 4;
        const auto next = std::min(pos + length, value.size());
        const auto candidate = clipped + value.substr(pos, next - pos) + "...";
        if (MeasureTextEx(font, candidate.c_str(), static_cast<float>(fontSize), 1.0f).x > maxWidth) break;
        clipped += value.substr(pos, next - pos);
        pos = next;
    }
    return clipped.empty() ? "..." : clipped + "...";
}

std::string Format(const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    char probe[512];
    const auto needed = std::vsnprintf(probe, sizeof(probe), pattern, args);
    va_end(args);
    if (needed < 0) return pattern;
    if (static_cast<size_t>(needed) < sizeof(probe)) return std::string(probe, static_cast<size_t>(needed));

    std::string grown(static_cast<size_t>(needed) + 1, '\0');
    va_start(args, pattern);
    std::vsnprintf(grown.data(), grown.size(), pattern, args);
    va_end(args);
    grown.resize(std::strlen(grown.data()));
    return grown;
}

}  // namespace ui
