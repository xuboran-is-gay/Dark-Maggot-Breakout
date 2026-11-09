#include "core.h"
#include <cstdio>
#include <sstream>

std::string utf8FromCodepoint(u32 cp) {
    std::string s;
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    } else {
        s += (char)(0xF0 | (cp >> 18));
        s += (char)(0x80 | ((cp >> 12) & 0x3F));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
    return s;
}

std::vector<u32> utf8Codepoints(const std::string& text) {
    std::vector<u32> out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) { out.push_back(c); ++i; continue; }
        int n = 0; u32 cp = 0;
        if ((c >> 5) == 0x6) { n = 2; cp = c & 0x1F; }
        else if ((c >> 4) == 0xE) { n = 3; cp = c & 0x0F; }
        else if ((c >> 3) == 0x1E) { n = 4; cp = c & 0x07; }
        else { out.push_back('?'); ++i; continue; }
        if (i + n > text.size()) { out.push_back('?'); break; }
        bool ok = true;
        for (int k = 1; k < n; ++k) {
            unsigned char cc = (unsigned char)text[i + k];
            if ((cc >> 6) != 0x2) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (ok) out.push_back(cp); else out.push_back('?');
        i += n;
    }
    return out;
}

std::string formatMoney(i64 value) {
    bool neg = value < 0;
    if (neg) value = -value;
    std::string digits = std::to_string(value);
    std::string out;
    int count = 0;
    for (int i = (int)digits.size() - 1; i >= 0; --i) {
        out.push_back(digits[(size_t)i]);
        if (++count % 3 == 0 && i > 0) out.push_back(',');
    }
    if (neg) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

std::string formatTime(float seconds) {
    int s = std::max(0, (int)std::ceil(seconds));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}
