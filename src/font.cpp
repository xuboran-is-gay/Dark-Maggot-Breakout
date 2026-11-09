#include "font.h"
#include <cstdio>
#include <cstring>
#include <fstream>

static u32 rdU32(const std::vector<u8>& d, size_t& p) {
    if (p + 4 > d.size()) return 0;
    u32 v = (u32)d[p] | ((u32)d[p + 1] << 8) | ((u32)d[p + 2] << 16) | ((u32)d[p + 3] << 24);
    p += 4;
    return v;
}
static int rdI32(const std::vector<u8>& d, size_t& p) { return (int)rdU32(d, p); }

bool Font::load(const std::string& path, SDL_Renderer* renderer) {
    destroy();
    std::ifstream f(path, std::ios::binary);
    if (!f) { SDL_Log("font: cannot open %s", path.c_str()); return false; }
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if (sz < 16) return false;
    f.seekg(0, std::ios::beg);
    std::vector<u8> data((size_t)sz);
    f.read((char*)data.data(), sz);
    if (std::memcmp(data.data(), "MGF2", 4) != 0) { SDL_Log("font: bad magic (need MGF2)"); return false; }
    size_t p = 4;
    u32 count = rdU32(data, p);
    if (count == 0 || count > 16) return false;
    for (u32 i = 0; i < count; ++i) {
        Atlas a;
        a.px = (int)rdU32(data, p);
        a.ascent = (int)rdU32(data, p);
        a.descent = (int)rdU32(data, p);
        a.w = (int)rdU32(data, p);
        a.h = (int)rdU32(data, p);
        u32 glyphs = rdU32(data, p);
        if (a.w <= 0 || a.h <= 0 || a.w > 4096 || a.h > 4096 || glyphs > 30000) return false;
        for (u32 g = 0; g < glyphs; ++g) {
            u32 cp = rdU32(data, p);
            Glyph gl;
            gl.x = rdI32(data, p); gl.y = rdI32(data, p);
            gl.w = rdI32(data, p); gl.h = rdI32(data, p);
            gl.bx = rdI32(data, p); gl.by = rdI32(data, p);
            gl.advance = rdI32(data, p);
            if (p > data.size()) return false;
            a.glyphs[cp] = gl;
        }
        u32 alphaSize = rdU32(data, p);
        u32 packedSize = rdU32(data, p);
        if (alphaSize != (u32)a.w * (u32)a.h || p + packedSize > data.size()) return false;
        std::vector<u8> alpha(alphaSize);
        size_t out = 0;
        size_t ip = p;
        while (out < alphaSize) {
            if (ip + 2 > p + packedSize) return false;
            u32 count = data[ip++];
            u8 value = data[ip++];
            if (count == 0 || out + count > alphaSize) return false;
            std::memset(alpha.data() + out, value, count);
            out += count;
        }
        std::vector<u8> rgba((size_t)alphaSize * 4);
        for (size_t i = 0; i < alphaSize; ++i) {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = alpha[i];
        }
        a.tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, a.w, a.h);
        if (!a.tex) { SDL_Log("font: texture failed: %s", SDL_GetError()); return false; }
        SDL_UpdateTexture(a.tex, nullptr, rgba.data(), a.w * 4);
        SDL_SetTextureBlendMode(a.tex, SDL_BLENDMODE_BLEND);
        p += packedSize;
        atlases_.push_back(std::move(a));
    }
    std::sort(atlases_.begin(), atlases_.end(), [](const Atlas& a, const Atlas& b) { return a.px < b.px; });
    return true;
}

void Font::destroy() {
    for (auto& a : atlases_) if (a.tex) SDL_DestroyTexture(a.tex);
    atlases_.clear();
}

const Font::Atlas* Font::pick(int px) const {
    if (atlases_.empty()) return nullptr;
    const Atlas* best = &atlases_[0];
    int bestDist = std::abs(best->px - px);
    for (auto& a : atlases_) {
        int d = std::abs(a.px - px);
        if (d < bestDist) { best = &a; bestDist = d; }
    }
    return best;
}

int Font::measure(int px, const std::string& text) const {
    const Atlas* a = pick(px);
    if (!a) return 0;
    int best = 0, cur = 0;
    for (u32 cp : utf8Codepoints(text)) {
        if (cp == '\n') { best = std::max(best, cur); cur = 0; continue; }
        auto it = a->glyphs.find(cp);
        cur += it == a->glyphs.end() ? a->px / 2 : it->second.advance;
    }
    return std::max(best, cur);
}

int Font::lineHeight(int px) const {
    const Atlas* a = pick(px);
    if (!a) return px;
    return a->ascent + a->descent + 1;
}
int Font::ascent(int px) const {
    const Atlas* a = pick(px);
    return a ? a->ascent : px;
}

void Font::drawLine(SDL_Renderer* r, const std::string& line, float x, float y, int px,
                    Col color, Align align, float boxW) const {
    const Atlas* a = pick(px);
    if (!a) return;
    int w = measure(px, line);
    float dx = x;
    if (align == ALIGN_CENTER) dx = x + (boxW - w) * .5f;
    else if (align == ALIGN_RIGHT) dx = x + boxW - w;
    SDL_SetTextureColorMod(a->tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(a->tex, color.a);
    float baseline = y + a->ascent;
    for (u32 cp : utf8Codepoints(line)) {
        auto it = a->glyphs.find(cp);
        if (it == a->glyphs.end()) {
            // Missing glyph: a small outlined box, never a blank surprise.
            SDL_Rect box{(int)dx + 1, (int)y + a->ascent - px + 2, px - 3, px - 3};
            SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
            SDL_RenderDrawRect(r, &box);
            dx += px * .55f;
            continue;
        }
        const Glyph& g = it->second;
        if (g.w > 0 && g.h > 0) {
            SDL_Rect src{g.x, g.y, g.w, g.h};
            SDL_Rect dst{(int)std::lround(dx + g.bx), (int)std::lround(baseline + g.by), g.w, g.h};
            SDL_RenderCopy(r, a->tex, &src, &dst);
        }
        dx += g.advance;
    }
}

void Font::draw(SDL_Renderer* r, const std::string& text, float x, float y, int px,
                Col color, Align align, float boxW) const {
    const Atlas* a = pick(px);
    if (!a) return;
    float lineY = y;
    std::string line;
    for (size_t i = 0; i <= text.size();) {
        if (i == text.size() || text[i] == '\n') {
            drawLine(r, line, x, lineY, px, color, align, boxW);
            line.clear();
            if (i == text.size()) break;
            lineY += lineHeight(px);
            ++i;
            continue;
        }
        size_t start = i;
        // consume one UTF-8 sequence (lead-byte ranges: 0xxxxxxx / 110xxxxx /
        // 1110xxxx / 11110xxx).  The old test compared the 3-byte lead against
        // 3 instead of 0xE, which swallowed an extra byte and made many short
        // CJK strings vanish entirely.
        unsigned char c = (unsigned char)text[i];
        size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        if (i + n > text.size()) n = text.size() - i;
        i += n;
        line.append(text, start, n);
    }
}

void Font::drawShadow(SDL_Renderer* r, const std::string& text, float x, float y, int px,
                      Col color, Align align, float boxW, int shadow) const {
    draw(r, text, x + shadow, y + shadow, px, Col(0, 0, 0, 150), align, boxW);
    draw(r, text, x, y, px, color, align, boxW);
}

std::vector<std::string> Font::wrap(int px, const std::string& text, float maxW) const {
    const Atlas* a = pick(px);
    std::vector<std::string> lines;
    if (!a) return lines;
    for (const std::string& para : [&]() {
        std::vector<std::string> v; std::string cur;
        for (char c : text) { if (c == '\n') { v.push_back(cur); cur.clear(); } else cur += c; }
        v.push_back(cur); return v;
    }()) {
        if (para.empty()) { lines.push_back(""); continue; }
        std::string line;
        float w = 0;
        for (u32 cp : utf8Codepoints(para)) {
            auto it = a->glyphs.find(cp);
            int adv = it == a->glyphs.end() ? a->px / 2 : it->second.advance;
            if (w + adv > maxW && !line.empty()) {
                // Keep closing punctuation with the line it belongs to instead of
                // starting the next line with a lone "。" or "，".
                const bool closing = cp == U'，' || cp == U'。' || cp == U'、' || cp == U'；' ||
                                     cp == U'：' || cp == U'！' || cp == U'？' || cp == U'）' ||
                                     cp == U'》' || cp == U'”' || cp == U'’' || cp == U',' ||
                                     cp == U'.' || cp == U';' || cp == U':' || cp == U'!' ||
                                     cp == U'?' || cp == U')' || cp == U']' || cp == U'}';
                if (!closing) {
                    lines.push_back(line);
                    line.clear();
                    w = 0;
                    if (cp == ' ') continue;
                }
            }
            line += utf8FromCodepoint(cp);
            w += adv;
            // A punctuation mark pushed the current line just over the limit; flush it.
            if (w > maxW && !line.empty() && line.size() > 1) {
                lines.push_back(line);
                line.clear();
                w = 0;
            }
        }
        lines.push_back(line);
    }
    return lines;
}

void Font::drawWrapped(SDL_Renderer* r, const std::string& text, float x, float y, int px,
                       Col color, float maxW, int maxLines, float lineGap) const {
    auto lines = wrap(px, text, maxW);
    int n = std::min((int)lines.size(), maxLines);
    for (int i = 0; i < n; ++i) {
        std::string line = lines[(size_t)i];
        if (i == maxLines - 1 && (int)lines.size() > maxLines) line += "…";
        draw(r, line, x, y + i * (lineHeight(px) + lineGap), px, color);
    }
}
