#!/usr/bin/env python3
"""Generate the game's compact CJK bitmap font atlas.

Scans source files for every used codepoint, so the shipped atlas stays tiny.
Output: assets/font.bin (custom format, read by src/font.cpp).
"""
import os, re, struct, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_CANDIDATES = [
    "/usr/share/fonts/google-noto-sans-cjk-vf-fonts/NotoSansCJK-VF.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
]
SIZES = [
    (14, "Medium"), (18, "Medium"), (24, "Medium"),
    (32, "Bold"), (48, "Bold"), (56, "Bold"),
]
EXTRA = (
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
    "·×—…“”‘’《》〈〉「」『』【】、。，！？：；（）¥￥←→↑↓■□●○◆◇★☆"
)

def find_font():
    for p in FONT_CANDIDATES:
        if os.path.exists(p):
            return p
    raise SystemExit("找不到中文字体，请安装 Noto Sans CJK 或修改 tools/gen_font.py")

def collect_chars():
    chars = set(EXTRA)
    for dirpath, dirs, files in os.walk(os.path.join(ROOT, "src")):
        for fn in files:
            if fn.endswith((".cpp", ".h", ".hpp", ".c")):
                text = open(os.path.join(dirpath, fn), encoding="utf-8", errors="ignore").read()
                chars.update(text)
    # Keep printable unicode only; drop control chars.
    chars = {c for c in chars if ord(c) >= 32 and not (0x7f <= ord(c) <= 0x9f)}
    return sorted(chars, key=ord)

def glyph_metrics(font, ch):
    adv = int(round(font.getlength(ch)))
    bbox = font.getbbox(ch, anchor="ls")
    if bbox is None:
        return adv, (0, 0, 0, 0)
    x0, y0, x1, y1 = bbox
    return adv, (x0, y0, max(0, x1 - x0), max(0, y1 - y0))

def rle_alpha(data):
    """Run-length encode alpha bytes. Counts are 1..255, values 0..255."""
    out = bytearray()
    n = len(data)
    i = 0
    while i < n:
        v = data[i]
        j = i + 1
        limit = min(n, i + 255)
        while j < limit and data[j] == v:
            j += 1
        out.append(j - i)
        out.append(v)
        i = j
    return bytes(out)

def try_pack(glyphs, widths, heights, width, pad):
    x = pad
    y = pad
    row_h = 0
    placements = []
    for w, h in zip(widths, heights):
        if w > width - pad * 2:
            return None
        if x + w + pad > width:
            x = pad
            y += row_h
            row_h = 0
        if y + h + pad > 4096:
            return None
        placements.append((x, y))
        x += w + pad
        row_h = max(row_h, h + pad)
    height = y + row_h
    if height <= 0 or height > 4096:
        return None
    return placements, width, height

def make_atlas(chars, path, size, weight):
    font = ImageFont.truetype(path, size, index=2)  # SC face
    try:
        font.set_variation_by_name(weight)
    except Exception:
        pass
    ascent, descent = font.getmetrics()
    glyphs = []
    for ch in chars:
        adv, (bx, by, w, h) = glyph_metrics(font, ch)
        glyphs.append({"ch": ch, "adv": adv, "bx": bx, "by": by, "w": w, "h": h})
    # Shelf pack with 2px transparent gutter. Try rectangular widths and
    # pick the smallest texture, not a forced square (this cuts VRAM a lot).
    pad = 2
    widths = [g["w"] + pad * 2 for g in glyphs]
    heights = [g["h"] + pad * 2 for g in glyphs]
    best = None
    for width in (512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2560, 3072, 3584, 4096):
        packed = try_pack(glyphs, widths, heights, width, pad)
        if not packed:
            continue
        placements, aw, ah = packed
        pixels = aw * ah
        if best is None or pixels < best[0]:
            best = (pixels, placements, aw, ah)
    if best is None:
        raise SystemExit("字体图集超出 4096x4096")
    _, placements, aw, ah = best
    atlas = Image.new("L", (aw, ah), 0)
    draw = ImageDraw.Draw(atlas)
    for g, (px, py) in zip(glyphs, placements):
        g["x"], g["y"] = px, py
        if g["w"] <= 0 or g["h"] <= 0:
            continue
        mask = Image.new("L", (g["w"], g["h"]), 0)
        md = ImageDraw.Draw(mask)
        # anchor=ls draws around the baseline; shift by -bbox origin.
        md.text((-g["bx"], -g["by"]), g["ch"], font=font, fill=255, anchor="ls")
        atlas.paste(mask, (px + pad, py + pad))
    return font, ascent, descent, atlas, glyphs

def main():
    out = os.path.join(ROOT, "assets", "font.bin")
    path = find_font()
    chars = collect_chars()
    print(f"font: {path}\nchars: {len(chars)}")
    atlases = []
    for size, weight in SIZES:
        font, ascent, descent, atlas, glyphs = make_atlas(chars, path, size, weight)
        atlases.append((size, ascent, descent, atlas, glyphs))
        print(f"  {size:2d}px {weight:6s} atlas={atlas.width}x{atlas.height} glyphs={len(glyphs)}")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"MGF2")
        f.write(struct.pack("<I", len(atlases)))
        for size, ascent, descent, atlas, glyphs in atlases:
            alpha = atlas.tobytes()
            packed = rle_alpha(alpha)
            f.write(struct.pack("<III", size, ascent, descent))
            f.write(struct.pack("<II", atlas.width, atlas.height))
            f.write(struct.pack("<I", len(glyphs)))
            for g in glyphs:
                f.write(struct.pack("<Iiii iiii".replace(" ", ""),
                                    ord(g["ch"]), g["x"] + 2, g["y"] + 2,
                                    g["w"], g["h"], g["bx"], g["by"], g["adv"]))
            f.write(struct.pack("<II", len(alpha), len(packed)))
            f.write(packed)
            print(f"  {size:2d}px alpha={len(alpha)/1024:.0f}KiB packed={len(packed)/1024:.0f}KiB")

    print(f"wrote {out} ({os.path.getsize(out)/1024:.1f} KiB)")

if __name__ == "__main__":
    main()
