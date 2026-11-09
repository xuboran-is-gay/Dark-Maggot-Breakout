#include "ui.h"

void UI::init(SDL_Renderer* renderer, Font* f, Input* input, Audio* a) {
    r = renderer; font = f; in = input; audio = a;
}

void UI::geom(const SDL_Vertex* v, int n, const int* idx, int idxCount) {
    if (!r || n <= 0) return;
    SDL_RenderGeometry(r, nullptr, v, n, idx, idxCount);
}

void UI::fill(Rect rc, Col c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect q{(int)std::lround(rc.x), (int)std::lround(rc.y), (int)std::lround(rc.w), (int)std::lround(rc.h)};
    SDL_RenderFillRect(r, &q);
}

void UI::fillF(float x, float y, float w, float h, Col c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_FRect q{x, y, w, h};
    SDL_RenderFillRectF(r, &q);
}

void UI::chamfer(Rect rc, float cut, Col c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (cut <= .5f || rc.w < cut * 2.2f || rc.h < cut * 2.2f) { fill(rc, c); return; }
    SDL_Color sc{c.r, c.g, c.b, c.a};
    SDL_Vertex v[8];
    const float xs[8] = {rc.x + cut, rc.right() - cut, rc.right(), rc.right(), rc.right() - cut, rc.x + cut, rc.x, rc.x};
    const float ys[8] = {rc.y, rc.y, rc.y + cut, rc.bottom() - cut, rc.bottom(), rc.bottom(), rc.bottom() - cut, rc.y + cut};
    for (int i = 0; i < 8; ++i) {
        v[i].position = SDL_FPoint{xs[i], ys[i]};
        v[i].color = sc;
        v[i].tex_coord = SDL_FPoint{0, 0};
    }
    const int idx[18] = {0,1,2, 0,2,3, 0,3,4, 0,4,5, 0,5,6, 0,6,7};
    geom(v, 8, idx, 18);
}

void UI::chamferOutline(Rect rc, float cut, Col c, float t) {
    if (cut <= .5f) {
        line({rc.x, rc.y}, {rc.right(), rc.y}, c, t);
        line({rc.right(), rc.y}, {rc.right(), rc.bottom()}, c, t);
        line({rc.right(), rc.bottom()}, {rc.x, rc.bottom()}, c, t);
        line({rc.x, rc.bottom()}, {rc.x, rc.y}, c, t);
        return;
    }
    Vec2 p[8] = {{rc.x + cut, rc.y}, {rc.right() - cut, rc.y}, {rc.right(), rc.y + cut},
                 {rc.right(), rc.bottom() - cut}, {rc.right() - cut, rc.bottom()},
                 {rc.x + cut, rc.bottom()}, {rc.x, rc.bottom() - cut}, {rc.x, rc.y + cut}};
    for (int i = 0; i < 8; ++i) line(p[i], p[(i + 1) % 8], c, t);
}

void UI::line(Vec2 a, Vec2 b, Col c, float t) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    Vec2 d = b - a;
    float len = length(d);
    if (len < .01f) return;
    Vec2 n{-d.y / len * t * .5f, d.x / len * t * .5f};
    SDL_Color sc{c.r, c.g, c.b, c.a};
    SDL_Vertex v[4];
    SDL_FPoint pts[4] = {{a.x + n.x, a.y + n.y}, {b.x + n.x, b.y + n.y},
                         {b.x - n.x, b.y - n.y}, {a.x - n.x, a.y - n.y}};
    for (int i = 0; i < 4; ++i) { v[i].position = pts[i]; v[i].color = sc; v[i].tex_coord = {0, 0}; }
    const int idx[6] = {0,1,2, 0,2,3};
    geom(v, 4, idx, 6);
}

void UI::circle(Vec2 c, float rad, Col col, int seg) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (rad <= 0) return;
    SDL_Color sc{col.r, col.g, col.b, col.a};
    std::vector<SDL_Vertex> v;
    v.reserve((size_t)seg + 2);
    v.push_back({{c.x, c.y}, sc, {0, 0}});
    for (int i = 0; i <= seg; ++i) {
        float a = (float)i / seg * 6.2831853f;
        v.push_back({{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad}, sc, {0, 0}});
    }
    std::vector<int> idx;
    idx.reserve((size_t)seg * 3);
    for (int i = 1; i <= seg; ++i) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }
    geom(v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void UI::ellipse(Vec2 c, float rx, float ry, Col col, int seg) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (rx <= 0 || ry <= 0) return;
    SDL_Color sc{col.r, col.g, col.b, col.a};
    std::vector<SDL_Vertex> v;
    v.reserve((size_t)seg + 2);
    v.push_back({{c.x, c.y}, sc, {0, 0}});
    for (int i = 0; i <= seg; ++i) {
        float a = (float)i / seg * 6.2831853f;
        v.push_back({{c.x + std::cos(a) * rx, c.y + std::sin(a) * ry}, sc, {0, 0}});
    }
    std::vector<int> idx;
    idx.reserve((size_t)seg * 3);
    for (int i = 1; i <= seg; ++i) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }
    geom(v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void UI::ring(Vec2 c, float rad, Col col, float t, int seg) {
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * 6.2831853f;
        float a1 = (float)(i + 1) / seg * 6.2831853f;
        line(c + fromAngle(a0, rad), c + fromAngle(a1, rad), col, t);
    }
}

void UI::gradient(Rect rc, Col top, Col bottom, int steps) {
    if (steps < 2) steps = 2;
    float h = rc.h / steps;
    for (int i = 0; i < steps; ++i) {
        Col c = mixCol(top, bottom, (float)i / (steps - 1));
        fillF(rc.x, rc.y + i * h, rc.w, h + .8f, c);
    }
}

void UI::panel(Rect rc, Col fillCol, Col border, float cut, bool shadow) {
    if (shadow) chamfer({rc.x + 3, rc.y + 4, rc.w, rc.h}, cut, Col(0, 0, 0, 120));
    chamfer(rc, cut, fillCol);
    chamferOutline(rc, cut, border, 1.2f);
    // A short accent notch in the top-left gives panels a fabricated feel.
    fill({rc.x + cut + 8, rc.y, 42, 2}, mixCol(border, C_ACCENT, .45f));
}

void UI::text(const std::string& s, float x, float y, int px, Col c, Align al, float boxW) {
    if (font) font->draw(r, s, x, y, px, c, al, boxW);
}

void UI::textShadow(const std::string& s, float x, float y, int px, Col c, Align al, float boxW, int sh) {
    if (font) font->drawShadow(r, s, x, y, px, c, al, boxW, sh);
}

void UI::wrapped(const std::string& s, float x, float y, int px, Col c, float maxW, int maxLines, float gap) {
    if (font) font->drawWrapped(r, s, x, y, px, c, maxW, maxLines, gap);
}

bool UI::hovered(Rect rc) const {
    return in && rc.contains(in->mouse);
}

bool UI::button(Rect rc, const std::string& label, bool enabled, bool primary, int px) {
    bool hov = enabled && hovered(rc);
    int id = rectId(rc);
    if (hov && hoverId_ != id && audio) audio->play(SFX_UI_MOVE, .18f, 1.08f);
    hoverId_ = hov ? id : (hoverId_ == id ? -1 : hoverId_);
    bool held = hov && in->down;
    float off = held ? 1.f : 0.f;
    Rect rr{rc.x, rc.y + off, rc.w, rc.h};
    float cut = 6.f;
    chamfer({rr.x + 2, rr.y + 3, rr.w, rr.h}, cut, Col(0, 0, 0, 110));
    Col fillc = C_PANEL2;
    Col border = C_BORDER;
    Col txt = C_TEXT;
    if (!enabled) {
        fillc = Col(16, 20, 15, 230); border = Col(42, 48, 36, 200); txt = C_MUTED.withA(130);
    } else if (primary) {
        fillc = hov ? Col(214, 250, 104, 255) : mixCol(C_ACCENT, C_BG, .28f);
        border = C_ACCENT;
        txt = C_INK;
    } else {
        fillc = hov ? Col(40, 51, 32, 245) : Col(20, 27, 18, 238);
        border = hov ? C_ACCENT : C_BORDER;
        txt = hov ? C_WHITE : C_TEXT;
    }
    chamfer(rr, cut, fillc);
    chamferOutline(rr, cut, border, 1.2f);
    // Left rail / focus tick.
    fill({rr.x, rr.y + cut, 3, rr.h - cut * 2}, primary ? C_INK.withA(150) : (hov ? C_ACCENT : C_ACCENT2.withA(130)));
    if (primary && hov) fill({rr.x, rr.y, rr.w, 1.5f}, C_WHITE.withA(120));
    int lh = font ? font->lineHeight(px) : px;
    text(label, rr.x + rr.w * .5f, rr.y + (rr.h - lh) * .5f - 1, px, txt, ALIGN_CENTER, 0);
    bool clicked = enabled && in->released && hovered(rc) && rc.contains(in->pressPos);
    if (clicked) { if (audio) audio->play(SFX_UI_CLICK, .8f, 1.f); return true; }
    return false;
}

bool UI::chip(Rect rc, const std::string& label, bool selected, bool enabled, int px) {
    bool hov = enabled && hovered(rc);
    int id = rectId(rc);
    if (hov && hoverId_ != id && audio) audio->play(SFX_UI_MOVE, .12f, 1.15f);
    if (hov) hoverId_ = id;
    Col fillc = selected ? C_ACCENT : (hov ? Col(40, 51, 32, 245) : Col(18, 24, 17, 230));
    Col border = selected ? C_ACCENT : (hov ? C_ACCENT2 : C_BORDER);
    Col txt = selected ? C_INK : (enabled ? C_TEXT : C_MUTED.withA(130));
    chamfer(rc, 4.f, fillc);
    chamferOutline(rc, 4.f, border, 1.f);
    int lh = font ? font->lineHeight(px) : px;
    text(label, rc.x + rc.w * .5f, rc.y + (rc.h - lh) * .5f - 1, px, txt, ALIGN_CENTER, 0);
    bool clicked = enabled && in->released && hovered(rc) && rc.contains(in->pressPos);
    if (clicked && audio) audio->play(SFX_UI_CLICK, .65f, 1.06f);
    return clicked;
}

void UI::bar(Rect rc, float t, Col fillCol, Col back, Col border) {
    t = clamp01(t);
    fill(rc, back);
    float w = (rc.w - 4) * t;
    if (w > 0) {
        fill({rc.x + 2, rc.y + 2, w, rc.h - 4}, fillCol);
        fill({rc.x + 2, rc.y + 2, w, std::max(1.f, (rc.h - 4) * .3f)}, mixCol(fillCol, C_WHITE, .28f));
    }
    chamferOutline(rc, 3.f, border, 1.f);
}

void UI::cursor(bool crosshairMode, Vec2 aim) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (!in) return;
    Vec2 p = crosshairMode ? aim : in->mouse;
    if (crosshairMode) { crosshair(p, 5.f, 7.f, C_ACCENT, false); return; }
    // A tiny bone-white pointer with an ink outline.
    Vec2 pts[3] = {p, p + Vec2(13, 4), p + Vec2(5, 15)};
    SDL_Color sc{C_WHITE.r, C_WHITE.g, C_WHITE.b, 255};
    SDL_Vertex v[3];
    for (int i = 0; i < 3; ++i) { v[i].position = SDL_FPoint{pts[i].x, pts[i].y}; v[i].color = sc; v[i].tex_coord = {0, 0}; }
    const int idx[3] = {0,1,2};
    geom(v, 3, idx, 3);
    line(pts[0], pts[1], C_INK, 1.2f);
    line(pts[1], pts[2], C_INK, 1.5f);
    line(pts[2], pts[0], C_INK, 1.2f);
}

void UI::crosshair(Vec2 p, float gap, float len, Col c, bool thick) {
    float t = thick ? 2.2f : 1.4f;
    line({p.x - gap - len, p.y}, {p.x - gap, p.y}, c, t);
    line({p.x + gap, p.y}, {p.x + gap + len, p.y}, c, t);
    line({p.x, p.y - gap - len}, {p.x, p.y - gap}, c, t);
    line({p.x, p.y + gap}, {p.x, p.y + gap + len}, c, t);
    fill({p.x - 1, p.y - 1, 2, 2}, c);
}

void UI::diamond(Vec2 c, float rad, Col col) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Color sc{col.r, col.g, col.b, col.a};
    SDL_Vertex v[4];
    SDL_FPoint pts[4] = {{c.x, c.y - rad}, {c.x + rad, c.y}, {c.x, c.y + rad}, {c.x - rad, c.y}};
    for (int i = 0; i < 4; ++i) { v[i].position = pts[i]; v[i].color = sc; v[i].tex_coord = {0, 0}; }
    const int idx[6] = {0,1,2, 0,2,3};
    geom(v, 4, idx, 6);
}

void UI::triangle(Vec2 c, float rad, float ang, Col col) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Color sc{col.r, col.g, col.b, col.a};
    SDL_Vertex v[3];
    for (int i = 0; i < 3; ++i) {
        Vec2 p = c + fromAngle(ang + i * 2.0943951f, rad);
        v[i].position = SDL_FPoint{p.x, p.y}; v[i].color = sc; v[i].tex_coord = {0, 0};
    }
    const int idx[3] = {0,1,2};
    geom(v, 3, idx, 3);
}

void UI::sectionTitle(const std::string& title, Vec2 p, float w) {
    text(title, p.x, p.y, 18, C_ACCENT);
    fill({p.x, p.y + 27, w, 1}, C_BORDER);
    fill({p.x, p.y + 27, 46, 2}, C_ACCENT2);
}


static SDL_Texture* makeTexture(SDL_Renderer* r, int w, int h, const std::vector<u8>& px) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

bool Visual::init(SDL_Renderer* r) {
    destroy();
    {
        const int N = 256;
        std::vector<u8> px((size_t)N * N * 4, 0);
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            float dx = (x + .5f) / N * 2 - 1, dy = (y + .5f) / N * 2 - 1;
            float d = std::sqrt(dx * dx + dy * dy);
            float a = std::pow(clamp01(1.f - d), 2.2f);
            size_t i = ((size_t)y * N + x) * 4;
            px[i] = 255; px[i + 1] = 255; px[i + 2] = 255;
            px[i + 3] = (u8)std::lround(a * 255);
        }
        glow = makeTexture(r, N, N, px);
        if (glow) SDL_SetTextureBlendMode(glow, SDL_BLENDMODE_ADD);
    }
    {
        const int W = 640, H = 400;
        std::vector<u8> px((size_t)W * H * 4, 0);
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float dx = (x + .5f) / W * 2 - 1, dy = (y + .5f) / H * 2 - 1;
            float d = clamp01(std::sqrt(dx * dx * .82f + dy * dy * 1.12f));
            float a = clamp01((d - .42f) / .58f);
            a = a * a * .78f;
            size_t i = ((size_t)y * W + x) * 4;
            px[i + 3] = (u8)std::lround(a * 255);
        }
        vignette = makeTexture(r, W, H, px);
    }
    {
        const int N = 256;
        std::vector<u8> px((size_t)N * N * 4);
        Rng rng(0xF00D1234u);
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            size_t i = ((size_t)y * N + x) * 4;
            u8 v = (u8)(rng.range(0, 255));
            px[i] = v; px[i + 1] = v; px[i + 2] = v; px[i + 3] = 255;
        }
        grain = makeTexture(r, N, N, px);
        if (grain) SDL_SetTextureBlendMode(grain, SDL_BLENDMODE_MOD);
    }
    return glow && vignette && grain;
}

void Visual::destroy() {
    if (glow) SDL_DestroyTexture(glow);
    if (vignette) SDL_DestroyTexture(vignette);
    if (grain) SDL_DestroyTexture(grain);
    glow = vignette = grain = nullptr;
}

void Visual::drawGlow(SDL_Renderer* r, Vec2 c, float rad, Col tint, float alpha) {
    if (!glow || alpha <= 0 || rad <= 0) return;
    SDL_SetTextureBlendMode(glow, SDL_BLENDMODE_ADD);
    SDL_SetTextureColorMod(glow, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(glow, (u8)std::lround(clamp01(alpha) * 255));
    SDL_FRect dst{c.x - rad, c.y - rad, rad * 2, rad * 2};
    SDL_RenderCopyF(r, glow, nullptr, &dst);
}

void Visual::drawVignette(SDL_Renderer* r, float alpha, Col tint) {
    if (!vignette) return;
    SDL_SetTextureBlendMode(vignette, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(vignette, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(vignette, (u8)std::lround(clamp01(alpha) * 255));
    SDL_FRect full{0, 0, VIEW_W, VIEW_H};
    SDL_RenderCopyF(r, vignette, nullptr, &full);
}

void Visual::drawGrain(SDL_Renderer* r, float alpha) {
    if (!grain || alpha <= 0) return;
    SDL_SetTextureBlendMode(grain, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(grain, 210, 220, 202);
    SDL_SetTextureAlphaMod(grain, (u8)std::lround(clamp01(alpha) * 255));
    for (int y = 0; y < 800; y += 256) for (int x = 0; x < 1280; x += 256) {
        SDL_Rect dst{x, y, 256, 256};
        SDL_RenderCopy(r, grain, nullptr, &dst);
    }
}
