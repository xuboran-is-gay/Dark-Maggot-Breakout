#pragma once
#include "core.h"
#include <unordered_map>

enum Align { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

class Font {
public:
    bool load(const std::string& path, SDL_Renderer* renderer);
    void destroy();
    bool ready() const { return !atlases_.empty(); }

    int measure(int px, const std::string& text) const;
    int lineHeight(int px) const;
    int ascent(int px) const;

    // Draws text with UTF-8 support. y is the top of the line box.
    void draw(SDL_Renderer* r, const std::string& text, float x, float y, int px,
              Col color, Align align = ALIGN_LEFT, float boxW = 0.f) const;
    void drawShadow(SDL_Renderer* r, const std::string& text, float x, float y, int px,
                    Col color, Align align = ALIGN_LEFT, float boxW = 0.f, int shadow = 2) const;
    // Wraps at pixel width. Newlines respected. Keeps CJK characters together per codepoint.
    std::vector<std::string> wrap(int px, const std::string& text, float maxW) const;
    void drawWrapped(SDL_Renderer* r, const std::string& text, float x, float y, int px,
                     Col color, float maxW, int maxLines = 99, float lineGap = 3.f) const;

private:
    struct Glyph {
        int x = 0, y = 0, w = 0, h = 0, bx = 0, by = 0, advance = 0;
    };
    struct Atlas {
        int px = 0, ascent = 0, descent = 0, w = 0, h = 0;
        SDL_Texture* tex = nullptr;
        std::unordered_map<u32, Glyph> glyphs;
    };
    std::vector<Atlas> atlases_;
    const Atlas* pick(int px) const;
    void drawLine(SDL_Renderer* r, const std::string& line, float x, float y, int px,
                  Col color, Align align, float boxW) const;
};
