#pragma once

#include <raylib.h>

#include <string>

namespace ui {

struct Palette {
    Color window;
    Color leftPanel;
    Color rightPanel;
    Color field;
    Color fieldBorder;
    Color accent;
    Color accentPressed;
    Color danger;
    Color text;
    Color muted;
    Color divider;
    Color rowHover;
    Color overlay;
};

extern const Palette kPalette;

// raylib draws with Rectangle; reuse it so widget rects need no conversion.
using Rect = Rectangle;

bool Contains(const Rect &rect, Vector2 point);
bool Button(const Rect &rect, const char *label, const Font &font, int fontSize, bool primary = false,
            bool danger = false, bool enabled = true);
void DrawLabel(const char *label, float x, float y, int fontSize, const Font &font, Color color);
std::string Elide(const Font &font, int fontSize, const std::string &value, float maxWidth);
std::string Format(const char *pattern, ...);

}  // namespace ui
