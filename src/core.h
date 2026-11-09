#pragma once
// 暗蛆突围 — core types and tiny helpers. No external deps beyond SDL2.
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

struct Col {
    Uint8 r = 255, g = 255, b = 255, a = 255;
    constexpr Col() = default;
    constexpr Col(Uint8 r_, Uint8 g_, Uint8 b_, Uint8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    Col withA(Uint8 alpha) const { return Col(r, g, b, alpha); }
    SDL_Color sdl() const { return SDL_Color{r, g, b, a}; }
};

constexpr Col C_INK      {  8,  11,   8, 255 };
constexpr Col C_BG       { 10,  14,  10, 255 };
constexpr Col C_PANEL    { 18,  24,  17, 245 };
constexpr Col C_PANEL2   { 25,  32,  22, 255 };
constexpr Col C_BORDER   { 55,  68,  45, 255 };
constexpr Col C_TEXT     {232, 229, 207, 255 };
constexpr Col C_MUTED    {138, 146, 124, 255 };
constexpr Col C_ACCENT   {198, 242,  78, 255 };
constexpr Col C_ACCENT2  {148, 196,  52, 255 };
constexpr Col C_GOLD     {232, 185,  76, 255 };
constexpr Col C_DANGER   {226,  87,  62, 255 };
constexpr Col C_CYAN     {101, 201, 181, 255 };
constexpr Col C_BLOOD    {124,  30,  26, 255 };
constexpr Col C_WHITE    {250, 248, 235, 255 };
constexpr Col C_BLACK    {0, 0, 0, 255};
constexpr float RPI = 3.14159265f;
constexpr float RTAU = 6.2831853f;

inline Col withAlpha(Col c, int a) { return c.withA((Uint8)std::clamp(a, 0, 255)); }

inline Col mixCol(Col a, Col b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return Col((Uint8)std::lround(a.r + (b.r - a.r) * t),
               (Uint8)std::lround(a.g + (b.g - a.g) * t),
               (Uint8)std::lround(a.b + (b.b - a.b) * t),
               (Uint8)std::lround(a.a + (b.a - a.a) * t));
}

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length2(Vec2 v) { return dot(v, v); }
inline float length(Vec2 v) { return std::sqrt(length2(v)); }
inline Vec2 normalize(Vec2 v) { float l = length(v); return l > 0.00001f ? v / l : Vec2(0, 1); }
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
inline float lerpF(float a, float b, float t) { return a + (b - a) * t; }
inline float clampF(float v, float lo, float hi) { return std::clamp(v, lo, hi); }
inline Vec2 rotateV(Vec2 v, float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
inline float angleOf(Vec2 v) { return std::atan2(v.y, v.x); }
inline Vec2 fromAngle(float rad, float len = 1.f) { return {std::cos(rad) * len, std::sin(rad) * len}; }

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Vec2 center() const { return {x + w * .5f, y + h * .5f}; }
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    bool overlaps(const Rect& o) const { return x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y; }
    SDL_Rect sdl() const {
        return SDL_Rect{(int)std::lround(x), (int)std::lround(y), (int)std::lround(w), (int)std::lround(h)};
    }
    Rect inflated(float d) const { return {x - d, y - d, w + d * 2, h + d * 2}; }
};

struct Rng {
    u64 s = 0x9E3779B97F4A7C15ull;
    explicit Rng(u64 seed = 0) { reseed(seed); }
    void reseed(u64 seed) {
        s = seed ? seed : 0x123456789ABCDEFull;
        for (int i = 0; i < 4; ++i) next();
    }
    u64 next() {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 0x2545F4914F6CDD1Dull;
    }
    u32 u32v() { return (u32)(next() >> 32); }
    int range(int lo, int hi) { if (hi <= lo) return lo; return lo + (int)(u32v() % (u32)(hi - lo + 1)); }
    float f01() { return (float)(next() >> 40) / 16777216.0f; }
    float rangeF(float lo, float hi) { return lo + (hi - lo) * f01(); }
    bool chance(float p) { return f01() < p; }
    template <typename T> const T& pick(const std::vector<T>& v) { return v[u32v() % v.size()]; }
};

inline u32 hash2(int x, int y, u32 seed = 0x51ED270Bu) {
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline float hash01(int x, int y, u32 seed = 0x51ED270Bu) {
    return (hash2(x, y, seed) & 0xFFFFFF) / 16777216.0f;
}

inline float dist(Vec2 a, Vec2 b) { return length(a - b); }
inline float dist2(Vec2 a, Vec2 b) { return length2(a - b); }
inline float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

// Segment/circle intersection. Returns distance along segment to first hit, or -1.
inline float rayCircle(Vec2 a, Vec2 b, Vec2 c, float r) {
    Vec2 d = b - a;
    Vec2 f = a - c;
    float A = dot(d, d);
    if (A < 1e-6f) return -1.f;
    float B = 2.f * dot(f, d);
    float C = dot(f, f) - r * r;
    float disc = B * B - 4.f * A * C;
    if (disc < 0) return -1.f;
    float sq = std::sqrt(disc);
    float t1 = (-B - sq) / (2.f * A);
    float t2 = (-B + sq) / (2.f * A);
    if (t1 >= 0 && t1 <= 1) return t1;
    if (t2 >= 0 && t2 <= 1) return t2;
    return -1.f;
}

struct Timer {
    float t = 0;
    void update(float dt) { t += dt; }
    bool passed(float interval) const { return t >= interval; }
    void reset() { t = 0; }
};

std::string formatMoney(i64 value);
std::string formatTime(float seconds);
std::string utf8FromCodepoint(u32 cp);
std::vector<u32> utf8Codepoints(const std::string& s);
