#pragma once
// Immediate-mode-ish UI helpers: input, panels, chamfered buttons, bars, cursors.
#include "core.h"
#include "font.h"
#include "audio.h"
#include <array>
#include <unordered_set>

constexpr float VIEW_W = 1280.f;
constexpr float VIEW_H = 800.f;

struct Input {
    Vec2 mouse;
    Vec2 pressPos;
    bool down = false;
    bool pressed = false;
    bool released = false;
    int wheel = 0;
    std::array<bool, SDL_NUM_SCANCODES> hit{};
    std::array<bool, SDL_NUM_SCANCODES> held{};
    std::unordered_set<SDL_Keycode> keysDown;
    std::unordered_set<SDL_Keycode> keysHit;
    const Uint8* state = nullptr;

    void beginFrame() {
        pressed = false;
        released = false;
        wheel = 0;
        hit.fill(false);
        keysHit.clear();
    }
    // SDL's keyboard-state array is indexed by physical scancode.  Some
    // layouts / remote desktops report letter keys only through keycodes, so
    // also track the event stream and accept either representation.
    bool key(SDL_Scancode sc) const {
        if (held[(size_t)sc] || (state && state[sc])) return true;
        SDL_Keycode kc = SDL_GetKeyFromScancode(sc);
        return kc && keysDown.count(kc) != 0;
    }
    bool keyHit(SDL_Scancode sc) const {
        if (hit[(size_t)sc]) return true;
        SDL_Keycode kc = SDL_GetKeyFromScancode(sc);
        return kc && keysHit.count(kc) != 0;
    }
    bool keyAny() const { return state != nullptr; }
};

struct Visual {
    SDL_Texture* glow = nullptr;
    SDL_Texture* vignette = nullptr;
    SDL_Texture* grain = nullptr;
    bool init(SDL_Renderer* r);
    void destroy();
    void drawGlow(SDL_Renderer* r, Vec2 c, float rad, Col tint, float alpha = 1.f);
    void drawVignette(SDL_Renderer* r, float alpha = 1.f, Col tint = Col(0, 0, 0));
    void drawGrain(SDL_Renderer* r, float alpha = .12f);
};

class UI {
public:
    SDL_Renderer* r = nullptr;
    Font* font = nullptr;
    Input* in = nullptr;
    Audio* audio = nullptr;

    void init(SDL_Renderer* renderer, Font* f, Input* input, Audio* a);

    void fill(Rect rc, Col c);
    void fillF(float x, float y, float w, float h, Col c);
    void chamfer(Rect rc, float cut, Col c);
    void chamferOutline(Rect rc, float cut, Col c, float t = 1.f);
    void line(Vec2 a, Vec2 b, Col c, float t = 1.f);
    void circle(Vec2 c, float rad, Col col, int seg = 20);
    void ellipse(Vec2 c, float rx, float ry, Col col, int seg = 22);
    void ring(Vec2 c, float rad, Col col, float t = 1.5f, int seg = 28);
    void gradient(Rect rc, Col top, Col bottom, int steps = 32);
    void panel(Rect rc, Col fillCol, Col border, float cut = 10.f, bool shadow = true);

    void text(const std::string& s, float x, float y, int px, Col c,
              Align al = ALIGN_LEFT, float boxW = 0.f);
    void textShadow(const std::string& s, float x, float y, int px, Col c,
                    Align al = ALIGN_LEFT, float boxW = 0.f, int sh = 2);
    void wrapped(const std::string& s, float x, float y, int px, Col c, float maxW,
                 int maxLines = 99, float gap = 3.f);

    bool hovered(Rect rc) const;
    bool button(Rect rc, const std::string& label, bool enabled = true,
                bool primary = false, int px = 18);
    bool chip(Rect rc, const std::string& label, bool selected, bool enabled = true, int px = 15);
    void bar(Rect rc, float t, Col fillCol, Col back = Col(8, 12, 8, 220), Col border = C_BORDER);

    void cursor(bool crosshairMode = false, Vec2 aim = Vec2(-1, -1));
    void crosshair(Vec2 p, float gap, float len, Col c, bool thick = false);
    void diamond(Vec2 c, float rad, Col col);
    void triangle(Vec2 c, float rad, float ang, Col col);

    // Utility for the common "label / value" heading.
    void sectionTitle(const std::string& title, Vec2 p, float w);

private:
    int hoverId_ = -1;
    static int rectId(Rect rc) {
        return (int)((rc.x * 7.f + rc.y * 13.f + rc.w * 17.f + rc.h * 19.f));
    }
    void geom(const SDL_Vertex* v, int n, const int* idx, int idxCount);
};
