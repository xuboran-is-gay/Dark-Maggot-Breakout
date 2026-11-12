#include "raid.h"
#include <algorithm>
#include <cmath>

namespace {
Col shade(Col c, float f) {
    return Col((Uint8)std::clamp((int)std::lround(c.r * f), 0, 255),
               (Uint8)std::clamp((int)std::lround(c.g * f), 0, 255),
               (Uint8)std::clamp((int)std::lround(c.b * f), 0, 255), c.a);
}
} // namespace

void Raid::render(UI& ui, Visual& visual, float time) {
    float shake = std::min(1.f, shake_ / 12.f);
    Vec2 sh(std::sin(time * 101.f) * shake * 4.f, std::cos(time * 87.f) * shake * 3.f);
    Vec2 cam = camera_ + sh;
    drawWorld(ui, cam);
    drawLighting(ui, visual, cam);
    visual.drawGrain(ui.r, .045f);
    drawHUD(ui, cam);
    drawEdgeMarkers(ui, cam);
    drawCrosshair(ui, cam);
    if (damageFlash_ > .01f) {
        ui.fillF(0, 0, VIEW_W, VIEW_H, Col(150, 18, 12, (Uint8)(damageFlash_ * 42.f)));
        visual.drawVignette(ui.r, damageFlash_ * .75f, Col(120, 10, 6));
    }
    if (mapOpen_) drawMapOverlay(ui);
    else if (paused_) drawPauseOverlay(ui);
    if (playerDead_) drawDeathOverlay(ui);
}

void Raid::drawWorld(UI& ui, Vec2 cam) {
    ui.fillF(0, 0, VIEW_W, VIEW_H, Col(7, 9, 7, 255));
    int ts = map_.tileSize;
    int x0 = std::max(0, (int)(cam.x / ts) - 1);
    int y0 = std::max(0, (int)(cam.y / ts) - 1);
    int x1 = std::min(map_.w - 1, x0 + (int)(VIEW_W / ts) + 3);
    int y1 = std::min(map_.h - 1, y0 + (int)(VIEW_H / ts) + 3);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) drawTile(ui, x, y, cam);

    drawDecals(ui, cam);
    drawExtraction(ui, cam);

    std::vector<const LootBox*> boxes;
    for (auto& b : boxes_) if (b.discovered) boxes.push_back(&b);
    std::sort(boxes.begin(), boxes.end(), [](const LootBox* a, const LootBox* b) { return a->pos.y < b->pos.y; });
    for (auto* b : boxes) drawBox(ui, *b, cam);

    std::vector<const Enemy*> drawable;
    for (auto& e : enemies_) {
        // Hostile maggots are always drawn, even in darkness and through walls.
        // Only expired corpses disappear; living enemies are never hidden by
        // the flashlight cone, fog-of-war or line-of-sight.
        if (!e.alive && e.deathTime > 1.5f) continue;
        drawable.push_back(&e);
    }
    std::sort(drawable.begin(), drawable.end(), [](const Enemy* a, const Enemy* b) { return a->pos.y < b->pos.y; });
    for (auto* e : drawable) drawEnemy(ui, *e, cam);
    drawPlayer(ui, cam);

    // Tracers glow like hot wire.
    for (const auto& t : tracers_) {
        float k = t.life / t.maxLife;
        Col c = withAlpha(t.color, (Uint8)(t.color.a * k));
        ui.line(t.a - cam, t.b - cam, withAlpha(c, (Uint8)(c.a * .35f)), t.width * 3.f);
        ui.line(t.a - cam, t.b - cam, c, t.width);
    }
    for (const auto& p : particles_) {
        float k = std::clamp(p.life / p.maxLife, 0.f, 1.f);
        Vec2 s = p.pos - cam;
        if (s.x < -20 || s.y < -20 || s.x > VIEW_W + 20 || s.y > VIEW_H + 20) continue;
        Col c = withAlpha(p.color, (Uint8)(p.color.a * k));
        if (p.kind == 1) {
            ui.ellipse(s, p.size * (.4f + k * .8f), p.size * (.25f + k * .45f), c, 8);
        } else if (p.kind == 2) {
            ui.circle(s, p.size * (1.7f - k), c, 10);
        } else if (p.kind == 3) {
            ui.fillF(s.x - p.size * .5f, s.y - p.size * .25f, p.size, p.size * .5f, c);
        } else {
            ui.line(s, s - normalize(p.vel) * (p.size * 2.f), c, std::max(1.f, p.size * .6f));
        }
    }
    for (const auto& f : floats_) {
        float k = f.life / f.maxLife;
        Vec2 s = f.pos - cam;
        ui.textShadow(f.text, s.x, s.y, f.px, withAlpha(f.color, (Uint8)(f.color.a * std::min(1.f, k * 2.5f))), ALIGN_CENTER, 0, 2);
    }
}

void Raid::drawTile(UI& ui, int x, int y, Vec2 cam) {
    const Tile& t = map_.at(x, y);
    if (!t.seen) return;
    int ts = map_.tileSize;
    Rect r{(float)x * ts - cam.x, (float)y * ts - cam.y, (float)ts, (float)ts};
    if (r.right() < -2 || r.bottom() < -2 || r.x > VIEW_W + 2 || r.y > VIEW_H + 2) return;
    bool v = tileVis(x, y);
    float dim = v ? 1.f : .58f;
    const MapDef& md = MAPS[mapIndex_];
    float h = hash01(x, y, (u32)map_.seed);
    Col wall = shade(md.wall, dim * (1.04f + h * .18f));
    Col floor = shade(md.floor, dim * (1.55f + h * .28f));

    if (t.type == T_WALL) {
        ui.fillF(r.x, r.y, r.w, r.h, shade(wall, .60f));
        ui.fillF(r.x + 1, r.y + 1, r.w - 2, r.h - 2, wall);
        ui.fillF(r.x + 2, r.y + 2, r.w - 4, 2.f, shade(wall, v ? 1.35f : 1.1f));
        ui.fillF(r.x + 2, r.y + r.h - 4.f, r.w - 4, 2.f, shade(wall, .55f));
        if (h > .74f) {
            ui.line({r.x + 7, r.y + 9}, {r.x + r.w - 8, r.y + r.h - 9}, shade(wall, .7f), 1.f);
        }
        if (h < .16f) ui.ellipse({r.x + r.w * .6f, r.y + r.h * .35f}, 5.f, 3.f, shade(wall, 1.35f), 7);
        return;
    }

    // Floors under everything, even props.
    ui.fillF(r.x, r.y, r.w, r.h, floor);
    ui.fillF(r.x, r.y, r.w, 1.f, shade(floor, 1.18f));
    ui.fillF(r.x, r.y, 1.f, r.h, shade(floor, 1.10f));
    ui.fillF(r.x, r.y + r.h - 1.f, r.w, 1.f, shade(floor, .70f));
    ui.fillF(r.x + r.w - 1.f, r.y, 1.f, r.h, shade(floor, .76f));
    if (h > .86f) {
        ui.ellipse({r.x + r.w * .55f, r.y + r.h * .58f}, 9.f, 4.5f, shade(floor, .72f), 10);
    } else if (h < .09f) {
        ui.circle({r.x + r.w * .32f, r.y + r.h * .38f}, 2.4f, withAlpha(md.accent, 55), 8);
    }

    if (t.type == T_GRATE) {
        ui.fillF(r.x + 2, r.y + 2, r.w - 4, r.h - 4, shade(floor, .62f));
        for (int i = 4; i < ts; i += 6) {
            ui.fillF(r.x + i, r.y + 3, 2.f, r.h - 6, shade(floor, .36f));
        }
        return;
    }
    if (t.type == T_CRATE) {
        Rect c{r.x + 5, r.y + 4, r.w - 10, r.h - 9};
        ui.fillF(c.x + 2, c.y + 3, c.w, c.h, Col(0, 0, 0, 80));
        ui.fillF(c.x, c.y, c.w, c.h, shade(Col(98, 77, 43), dim));
        ui.fillF(c.x + 1, c.y + 1, c.w - 2, 2, shade(Col(132, 105, 59), dim));
        ui.fillF(c.x, c.y, 2, c.h, shade(Col(56, 43, 27), dim));
        ui.fillF(c.x + c.w - 2, c.y, 2, c.h, shade(Col(58, 45, 28), dim));
        ui.line({c.x + 3, c.y + 4}, {c.x + c.w - 4, c.y + c.h - 5}, shade(Col(61, 46, 26), dim), 1.4f);
        ui.line({c.x + c.w - 4, c.y + 4}, {c.x + 3, c.y + c.h - 5}, shade(Col(61, 46, 26), dim), 1.4f);
        ui.fillF(c.x + c.w * .5f - 2, c.y, 4, c.h, withAlpha(md.accent, v ? 60 : 20));
        return;
    }
    if (t.type == T_BARREL) {
        Vec2 cc{r.x + r.w * .5f, r.y + r.h * .52f};
        ui.ellipse(cc + Vec2(2, 4), 10, 5, Col(0, 0, 0, 85), 12);
        ui.circle(cc, 10.f, shade(Col(63, 70, 44), dim), 14);
        ui.circle(cc, 7.4f, shade(Col(83, 93, 55), dim), 14);
        ui.ring(cc, 10.f, shade(Col(38, 44, 28), dim), 1.3f, 14);
        ui.circle(cc + Vec2(-2, -3), 2.3f, withAlpha(md.accent, v ? 140 : 50), 8);
        ui.line(cc + Vec2(-4, 6), cc + Vec2(4, 6), shade(Col(35, 39, 24), dim), 1.2f);
        return;
    }
    if (t.type == T_PILLAR) {
        Rect c{r.x + 8, r.y + 6, r.w - 16, r.h - 12};
        ui.fillF(c.x + 3, c.y + 5, c.w, c.h, Col(0, 0, 0, 95));
        ui.fillF(c.x, c.y, c.w, c.h, shade(Col(54, 52, 44), dim));
        ui.fillF(c.x + 2, c.y + 2, 3, c.h - 4, shade(Col(75, 72, 59), dim));
        ui.fillF(c.x + c.w - 4, c.y + 2, 3, c.h - 4, shade(Col(33, 32, 27), dim));
        ui.fillF(c.x + c.w * .35f, c.y + 4, 3, c.h - 8, withAlpha(md.accent, v ? 40 : 12));
        return;
    }
}

void Raid::drawDecals(UI& ui, Vec2 cam) {
    int ts = map_.tileSize;
    for (const auto& d : decals_) {
        int tx = (int)(d.pos.x / ts), ty = (int)(d.pos.y / ts);
        if (!tileSeen(tx, ty)) continue;
        Vec2 s = d.pos - cam;
        if (s.x < -30 || s.y < -30 || s.x > VIEW_W + 30 || s.y > VIEW_H + 30) continue;
        float dim = tileVis(tx, ty) ? 1.f : .4f;
        ui.ellipse(s, d.rx, d.ry, withAlpha(shade(d.color, dim), d.color.a), 11);
    }
}

void Raid::drawExtraction(UI& ui, Vec2 cam) {
    const MapDef& md = MAPS[mapIndex_];
    for (int i = 0; i < (int)map_.extracts.size(); ++i) {
        const auto& ex = map_.extracts[(size_t)i];
        Rect z{ex.zone.x - cam.x, ex.zone.y - cam.y, ex.zone.w, ex.zone.h};
        if (z.right() < 0 || z.bottom() < 0 || z.x > VIEW_W || z.y > VIEW_H) continue;
        bool active = extractIndex_ == i;
        float pulse = .5f + .5f * std::sin(animTime_ * 3.2f + i);
        Col ac = active ? C_ACCENT : md.accent;
        ui.ellipse(z.center(), z.w * .47f, z.h * .43f, withAlpha(ac, (Uint8)(active ? 42 : 20)), 24);
        ui.ring(z.center(), z.w * .43f, withAlpha(ac, 120), 1.5f, 28);
        ui.ring(z.center(), z.w * (.43f + pulse * .07f), withAlpha(ac, (Uint8)(70 - pulse * 35)), 1.f, 28);
        // Four corner ticks.
        Vec2 c = z.center();
        float rr = z.w * .43f;
        for (int k = 0; k < 4; ++k) {
            float a = k * RPI * .5f + RPI * .25f;
            Vec2 p = c + fromAngle(a, rr);
            ui.line(p + fromAngle(a + RPI, 9.f), p + fromAngle(a, 9.f), ac, 2.f);
        }
        ui.textShadow("撤离", c.x, c.y - 9, 18, ac, ALIGN_CENTER, 0, 2);
        ui.text(ex.name, c.x, c.y + 11, 14, C_TEXT, ALIGN_CENTER);
    }
}

void Raid::drawBox(UI& ui, const LootBox& b, Vec2 cam) {
    int tx = (int)(b.pos.x / map_.tileSize), ty = (int)(b.pos.y / map_.tileSize);
    if (!tileSeen(tx, ty)) return;
    Vec2 s = b.pos - cam;
    if (s.x < -50 || s.y < -50 || s.x > VIEW_W + 50 || s.y > VIEW_H + 50) return;
    bool vis = tileVis(tx, ty);
    bool empty = b.items.empty();
    float dim = vis ? 1.f : .5f;
    bool near = dist(b.pos, player_.pos) < 96.f;
    float pulse = .5f + .5f * std::sin(animTime_ * 3.f + b.pulse);
    Col accent = b.kind == 4 ? C_GOLD : (b.kind == 1 ? C_WHITE : (b.kind == 2 ? C_ACCENT : C_GOLD));
    if (!empty) {
        ui.ellipse(s + Vec2(0, 4), 24 + pulse * 4, 8, withAlpha(accent, (Uint8)(10 + pulse * 12)), 12);
        if (near) {
            ui.ring(s + Vec2(0, -2), 15 + pulse * 2, withAlpha(accent, 90), 1.f, 16);
            ui.diamond(s + Vec2(0, -30 - pulse * 3), 4.5f, withAlpha(accent, 210));
        }
    }

    if (b.kind == 3 || b.kind == 4) {
        // Maggot corpse / boss carcass.
        float sc = b.kind == 4 ? 1.35f : .8f;
        ui.ellipse(s + Vec2(1, 5), 19 * sc, 7 * sc, Col(0, 0, 0, 90), 12);
        ui.ellipse(s, 17 * sc, 7 * sc, shade(Col(111, 95, 67), dim), 14);
        ui.circle(s + Vec2(-3 * sc, 0), 6.5f * sc, shade(Col(139, 118, 84), dim), 12);
        ui.circle(s + Vec2(5 * sc, -2), 5.5f * sc, shade(Col(103, 86, 60), dim), 12);
        ui.line(s + Vec2(-14 * sc, 2), s + Vec2(-22 * sc, 6), shade(Col(83, 69, 48), dim), 3.f);
        if (b.kind == 4) {
            ui.circle(s + Vec2(7, -2), 8.f, shade(Col(148, 90, 54), dim), 12);
            ui.ring(s + Vec2(7, -2), 9.f, withAlpha(C_GOLD, 130), 1.4f, 12);
        }
        return;
    }

    Col base = b.kind == 1 ? Col(196, 195, 176) : (b.kind == 2 ? Col(76, 96, 53) : Col(111, 85, 46));
    if (b.searched && empty) base = shade(base, .45f);
    base = shade(base, dim);
    ui.fillF(s.x - 17, s.y + 2, 34, 15, Col(0, 0, 0, 90));
    ui.fillF(s.x - 16, s.y - 14, 32, 28, shade(base, .72f));
    ui.fillF(s.x - 14, s.y - 13, 28, 24, base);
    ui.fillF(s.x - 14, s.y - 13, 28, 2, shade(base, 1.35f));
    ui.fillF(s.x - 14, s.y - 13, 2, 24, shade(base, 1.2f));
    ui.fillF(s.x + 12, s.y - 13, 2, 24, shade(base, .55f));
    ui.fillF(s.x - 3, s.y - 9, 6, 16, shade(base, .62f));
    if (b.kind == 1) {
        ui.fillF(s.x - 5, s.y - 5, 10, 3, accent);
        ui.fillF(s.x - 1.5f, s.y - 9, 3, 11, accent);
    } else if (b.kind == 2) {
        ui.fillF(s.x - 8, s.y - 6, 16, 9, C_ACCENT.withA(190));
        ui.line({s.x - 8, s.y + 5}, {s.x + 8, s.y + 5}, C_ACCENT, 2.f);
    } else {
        ui.ring({s.x, s.y - 1}, 5.5f, withAlpha(accent, 150), 1.2f, 12);
    }
    if (b.searched) {
        ui.fillF(s.x - 16, s.y - 14, 32, 5, shade(base, .55f));
    }
    if (near && !empty) {
        ui.textShadow("E 搜索", s.x, s.y - 31, 14, C_TEXT, ALIGN_CENTER, 0, 2);
    }
}

void Raid::drawEnemy(UI& ui, const Enemy& e, Vec2 cam) {
    Vec2 s = e.pos - cam;
    if (s.x < -70 || s.y < -80 || s.x > VIEW_W + 70 || s.y > VIEW_H + 80) return;
    if (!e.alive) {
        float k = std::clamp(1.f - e.deathTime / 1.4f, 0.f, 1.f);
        ui.ellipse(s + Vec2(0, 5), 18, 7, Col(70, 14, 12, (Uint8)(110 * k)), 10);
        drawMaggot(ui, s, e.angle + .6f, e.boss ? 1.5f : 1.f, Col(91, 73, 52), Col(48, 37, 28),
                   e.anim, e.boss, true);
        return;
    }
    bool boss = e.boss;
    float scale = boss ? 1.55f : (e.type == EN_BRUTE ? 1.2f : (e.type == EN_MERC ? 1.02f : .94f));
    Col body = ENEMIES[(int)e.type].color;
    if (e.hitFlash > 0) body = mixCol(body, C_WHITE, .7f);
    if (e.enraged) body = mixCol(body, C_DANGER, .24f);

    ui.ellipse(s + Vec2(1, 8), 19 * scale, 7 * scale, Col(0, 0, 0, 100), 14);
    if (boss) {
        float pulse = .5f + .5f * std::sin(animTime_ * 4.f);
        ui.ellipse(s + Vec2(0, 6), 30 + pulse * 4, 13, withAlpha(C_DANGER, 20), 18);
    }
    drawMaggot(ui, s, e.angle, scale, body, shade(body, .48f), e.anim, boss, false);
    drawGun(ui, s + fromAngle(e.angle, 8.f * scale), e.angle, boss ? WPN_SHOTGUN : (e.type == EN_MERC ? WPN_SMG : (e.type == EN_BRUTE ? WPN_SHOTGUN : WPN_PISTOL)), scale, Col(48, 42, 34));
    if (e.muzzle > 0) {
        Vec2 tip = s + fromAngle(e.angle, (boss ? 31.f : 21.f) * scale);
        ui.circle(tip, 5.f + e.muzzle * 90.f, Col(255, 204, 100, 190), 7);
        ui.line(tip, tip + fromAngle(e.angle + 2.5f, 14.f), Col(255, 198, 90, 170), 1.4f);
        ui.line(tip, tip + fromAngle(e.angle - 2.5f, 14.f), Col(255, 198, 90, 170), 1.4f);
    }

    if (e.alert) {
        float bob = std::sin(animTime_ * 5.f + e.anim) * 2.f;
        ui.textShadow("!", s.x, s.y - 34 * scale - 12 + bob, boss ? 26 : 20, C_DANGER, ALIGN_CENTER, 0, 2);
        ui.triangle({s.x, s.y - 21 * scale - 2 + bob}, 4.f, RPI * .5f, withAlpha(C_DANGER, 220));
    }
    if (e.hp < e.maxHp || e.alert) {
        float w = boss ? 54.f : 32.f;
        Rect br{s.x - w * .5f, s.y - 29 * scale - 8, w, boss ? 5.f : 3.5f};
        ui.bar(br, (float)e.hp / std::max(1, e.maxHp), e.enraged ? C_DANGER : (boss ? C_GOLD : C_BLOOD),
               Col(8, 6, 6, 210), Col(0, 0, 0, 0));
    }
    if (boss) {
        ui.textShadow("蛆王", s.x, s.y - 66, 15, C_GOLD, ALIGN_CENTER, 0, 2);
    }
}

void Raid::drawMaggot(UI& ui, Vec2 p, float angle, float scale, Col body, Col dark,
                      float phase, bool boss, bool dead) {
    Vec2 dir = fromAngle(angle, 1.f);
    Vec2 perp = fromAngle(angle + RPI * .5f, 1.f);
    float sc = scale * (boss ? 1.05f : 1.f);
    if (dead) {
        ui.ellipse(p + Vec2(0, 3), 21 * scale, 8 * scale, Col(66, 54, 39, 90), 14);
        for (int i = 0; i < 4; ++i) {
            float off = (-12.f + i * 8.f) * scale;
            Vec2 q = p + perp * off + dir * (3.f + i * 2.f) + Vec2(0, 4);
            ui.circle(q, (7.f - i * .8f) * scale, dark, 10);
        }
        ui.circle(p + dir * 16.f * scale + Vec2(0, 2), 6.f * scale, shade(body, .7f), 10);
        return;
    }
    // Overlapping segments; slight sinusoidal wiggle sells the crawl.
    for (int i = 0; i < 5; ++i) {
        float t = i / 4.f;
        float off = (-15.f + i * 7.4f) * sc;
        float wig = std::sin(phase * 8.f + i * .9f) * 1.7f * sc;
        Vec2 q = p + dir * off + perp * wig;
        float r = (6.4f + std::sin(t * RPI) * 4.4f) * sc;
        ui.circle(q + Vec2(1, 2), r + 1.5f, Col(0, 0, 0, 70), 12);
        ui.circle(q, r, dark, 14);
        ui.circle(q - Vec2(0, 1.2f), r * .82f, body, 14);
        ui.ellipse(q - Vec2(0, r * .34f), r * .50f, r * .24f, withAlpha(shade(body, 1.32f), 160), 9);
        if (i > 0 && i < 4) {
            ui.line(q - perp * r * .6f, q + perp * r * .6f, withAlpha(dark, 120), 1.2f);
        }
    }
    Vec2 head = p + dir * 18.f * sc;
    float hr = (boss ? 10.f : 8.2f) * sc;
    ui.circle(head, hr + 1.3f, Col(0, 0, 0, 90), 14);
    ui.circle(head, hr, shade(body, 1.14f), 16);
    ui.ellipse(head - dir * 2.f, hr * .58f, hr * .40f, withAlpha(shade(body, 1.45f), 170), 10);
    // Mandibles.
    Vec2 left = head + dir * hr * .62f + perp * hr * .65f;
    Vec2 right = head + dir * hr * .62f - perp * hr * .65f;
    ui.line(head + perp * hr * .4f, left + dir * 4.f, dark, boss ? 2.4f : 1.8f);
    ui.line(head - perp * hr * .4f, right + dir * 4.f, dark, boss ? 2.4f : 1.8f);
    // Eyes.
    ui.circle(head + dir * 2.f + perp * 2.6f, boss ? 2.1f : 1.5f, boss ? C_GOLD : C_DANGER, 7);
    ui.circle(head + dir * 2.f - perp * 2.6f, boss ? 2.1f : 1.5f, boss ? C_GOLD : C_DANGER, 7);
    if (boss) {
        ui.ring(head, hr + 3.f, withAlpha(C_GOLD, 150), 1.3f, 16);
        for (int i = 0; i < 5; ++i) {
            float a = -1.1f + i * .55f;
            Vec2 sp = head + fromAngle(a, hr + 1.f);
            ui.triangle(sp + fromAngle(a, 4.f), 3.f, a + RPI * .5f, C_GOLD.withA(190));
        }
    }
    (void)RTAU;
}

void Raid::drawPlayer(UI& ui, Vec2 cam) {
    Vec2 s = player_.pos - cam;
    float scale = 1.06f;
    if (playerDead_) {
        float k = std::clamp(1.f - deathTimer_ / 1.8f, 0.f, 1.f);
        ui.ellipse(s + Vec2(0, 6), 26, 10, Col(46, 8, 7, (Uint8)(100 + k * 80)), 14);
        drawMaggot(ui, s + Vec2(0, 3), player_.angle + .8f, scale, Col(104, 117, 55), Col(48, 57, 26),
                   player_.walkPhase, false, true);
        return;
    }
    float pulse = .5f + .5f * std::sin(animTime_ * 2.6f);
    if (player_.lightOn) {
        ui.ellipse(s, 29, 12, withAlpha(C_ACCENT, 12), 18);
    }
    ui.ellipse(s + Vec2(1, 9), 20, 7, Col(0, 0, 0, 105), 14);

    // Backpack silhouette.
    if (backpack_ > 0) {
        Vec2 bp = s - fromAngle(player_.angle, 15.f) + fromAngle(player_.angle + RPI * .5f, 1.f);
        float bs = 8.f + backpack_ * 1.7f;
        ui.circle(bp, bs + 1.f, Col(0, 0, 0, 80), 12);
        ui.circle(bp, bs, shade(BACKPACKS[backpack_].accent, .75f), 12);
        ui.circle(bp - Vec2(0, 2), bs * .62f, BACKPACKS[backpack_].accent, 10);
    }
    drawMaggot(ui, s, player_.angle, scale, Col(190, 216, 98), Col(76, 99, 39), player_.walkPhase, false, false);

    // Armor plate strapped across the middle.
    if (armor_ > 0) {
        Vec2 c = s + fromAngle(player_.angle, 2.f);
        Vec2 d = fromAngle(player_.angle, 1.f), pp = fromAngle(player_.angle + RPI * .5f, 1.f);
        Col ac = ARMORS[armor_].accent;
        ui.line(c - d * 7 + pp * 7, c + d * 6 + pp * 7, withAlpha(ac, 210), 5.f);
        ui.line(c - d * 7 - pp * 7, c + d * 6 - pp * 7, withAlpha(ac, 210), 5.f);
        ui.circle(c + d * 4, 3.f, withAlpha(C_WHITE, 100), 8);
    }

    if (knifeOut_) {
        drawKnife(ui, s + fromAngle(player_.angle, 8.f), player_.angle, 1.0f, Col(180, 194, 166));
    } else {
        drawGun(ui, s + fromAngle(player_.angle, 9.f), player_.angle, weapon_, 1.0f, Col(44, 38, 30));
    }
    if (!knifeOut_ && player_.muzzle > 0) {
        Vec2 tip = s + fromAngle(player_.angle, 30.f);
        ui.circle(tip, 7.f + player_.muzzle * 120.f, Col(255, 219, 108, 210), 8);
        for (int i = 0; i < 3; ++i) {
            float a = player_.angle + (i - 1) * .35f;
            ui.line(tip, tip + fromAngle(a, 15.f + player_.muzzle * 100.f), Col(255, 208, 96, 170), 1.6f);
        }
    }
    if (player_.reloading) {
        float k = 1.f - player_.reloadT / std::max(.01f, WEAPONS[weapon_].reload);
        ui.ring(s, 26.f, withAlpha(C_ACCENT, 150), 2.5f, 24);
        ui.ring(s, 26.f, withAlpha(C_ACCENT, 230), 2.5f, 24);
        (void)k;
    }
    if (player_.healT > 0) {
        ui.ring(s, 30.f + pulse * 2.f, withAlpha(C_ACCENT, 110), 1.4f, 22);
        ui.textShadow(player_.healKind == 1 ? "包扎" : "注射", s.x, s.y - 38, 14, C_ACCENT, ALIGN_CENTER, 0, 2);
    }
    if (player_.hp < 34.f) {
        float p = .5f + .5f * std::sin(animTime_ * 5.f);
        ui.ring(s, 31.f + p * 3.f, withAlpha(C_DANGER, (Uint8)(70 + p * 60)), 1.2f, 20);
    }
    (void)scale;
}

void Raid::drawGun(UI& ui, Vec2 p, float angle, int weapon, float scale, Col col) {
    Vec2 d = fromAngle(angle, 1.f);
    Vec2 pp = fromAngle(angle + RPI * .5f, 1.f);
    float len = 12.f, thick = 4.f;
    if (weapon == WPN_SMG) { len = 18.f; thick = 4.6f; }
    else if (weapon == WPN_SHOTGUN) { len = 23.f; thick = 5.4f; }
    else if (weapon == WPN_RIFLE) { len = 29.f; thick = 4.4f; }
    len *= scale; thick *= scale;
    Col light = shade(col, 1.6f);
    ui.line(p, p + d * len, col, thick + 1.5f);
    ui.line(p + pp * 1.2f, p + d * len + pp * 1.2f, light, 1.2f);
    // Grip and stock.
    ui.line(p - pp * 2.5f, p - d * 4.f - pp * 4.2f, shade(col, .8f), 2.8f * scale);
    if (weapon != WPN_PISTOL) {
        ui.line(p + d * (len * .24f), p + d * (len * .24f) + pp * 5.f, shade(col, .75f), 3.2f * scale);
        ui.line(p - d * 3.f, p - d * 7.f, shade(col, .82f), thick * 1.1f);
    }
    if (weapon == WPN_RIFLE) {
        ui.line(p + d * 8.f, p + d * 15.f, shade(col, 1.35f), 2.f * scale);
        ui.circle(p + d * 10.f, 2.2f * scale, Col(34, 46, 30, 230), 8);
    }
    if (weapon == WPN_SHOTGUN) {
        ui.line(p + d * 5.f - pp * 2.5f, p + d * (len - 2.f) - pp * 2.5f, shade(col, .9f), 1.8f);
    }
}

void Raid::drawKnife(UI& ui, Vec2 p, float angle, float scale, Col col) {
    Vec2 d = fromAngle(angle, 1.f);
    Vec2 pp = fromAngle(angle + RPI * .5f, 1.f);
    // Dark grip, pale chitin blade, bright edge.
    ui.line(p, p + d * 9.f * scale, Col(54, 48, 38), 5.f * scale);
    ui.line(p + pp * 2.f, p + pp * 4.f - d * 2.f, Col(78, 68, 50), 2.4f * scale);
    ui.line(p + d * 8.f * scale, p + d * 29.f * scale, shade(col, .72f), 3.2f * scale);
    ui.line(p + d * 9.f * scale + pp * .7f, p + d * 29.f * scale + pp * .7f, col, 1.2f * scale);
    ui.line(p + d * 27.f * scale, p + d * 27.f * scale + pp * 4.f, Col(225, 235, 210, 210), 1.2f * scale);
}

void Raid::drawFlashlightCone(UI& ui, Vec2 origin, float angle, float range, float half, Col tint) {
    if (!ui.r) return;
    const int rays = 38;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(rays + 1);
    Vec2 worldOrigin = origin + camera_;
    SDL_Vertex center;
    center.position = {origin.x, origin.y};
    center.color = SDL_Color{tint.r, tint.g, tint.b, (Uint8)std::min(255, tint.a * 2)};
    center.tex_coord = {0, 0};
    verts.push_back(center);
    for (int i = 0; i <= rays; ++i) {
        float a = angle - half + (2.f * half) * (i / (float)rays);
        Vec2 wend = worldOrigin + fromAngle(a, range);
        float t = map_.wallRay(worldOrigin, wend);
        if (t < 1.f) wend = lerp(worldOrigin, wend, std::max(0.f, t - .015f));
        Vec2 end = wend - camera_;
        SDL_Vertex v;
        v.position = {end.x, end.y};
        v.color = SDL_Color{tint.r, tint.g, tint.b, 0};
        v.tex_coord = {0, 0};
        verts.push_back(v);
    }
    for (int i = 1; i <= rays; ++i) {
        idx.push_back(0);
        idx.push_back(i);
        idx.push_back(i + 1);
    }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(ui.r, nullptr, verts.data(), (int)verts.size(), idx.data(), (int)idx.size());
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
}

void Raid::drawLighting(UI& ui, Visual& visual, Vec2 cam) {
    // Ambient darkness. Explored-but-not-visible tiles are already dimmed by the tile pass.
    ui.fillF(0, 0, VIEW_W, VIEW_H, Col(3, 7, 4, 28));
    const MapDef& md = MAPS[mapIndex_];
    for (Vec2 lp : map_.lamps) {
        int tx = (int)(lp.x / map_.tileSize), ty = (int)(lp.y / map_.tileSize);
        if (!tileSeen(tx, ty)) continue;
        Vec2 s = lp - cam;
        if (s.x < -130 || s.y < -130 || s.x > VIEW_W + 130 || s.y > VIEW_H + 130) continue;
        if (tileVis(tx, ty)) {
            visual.drawGlow(ui.r, s, 190.f, md.accent, .30f);
            ui.circle(s, 3.f, withAlpha(md.accent, 180), 8);
        }
    }
    Vec2 ps = player_.pos - cam;
    if (playerDead_) {
        visual.drawVignette(ui.r, .8f, Col(50, 0, 0));
        return;
    }
    visual.drawGlow(ui.r, ps, 240.f, Col(150, 178, 118), .13f);
    visual.drawGlow(ui.r, ps, 470.f, Col(95, 122, 84), .045f);
    if (player_.lightOn) {
        drawFlashlightCone(ui, ps, player_.angle, 760.f, .52f, Col(240, 222, 160, 42));
        drawFlashlightCone(ui, ps, player_.angle, 620.f, .32f, Col(255, 239, 190, 26));
        visual.drawGlow(ui.r, ps, 104.f, Col(219, 236, 150), .24f);
    } else {
        visual.drawGlow(ui.r, ps, 86.f, Col(172, 200, 110), .12f);
    }
    visual.drawVignette(ui.r, .50f);
}

void Raid::drawHUD(UI& ui, Vec2 cam) {
    (void)cam;
    const MapDef& md = MAPS[mapIndex_];
    // Location / risk.
    ui.panel({24, 20, 360, 80}, Col(14, 19, 13, 228), C_BORDER, 8);
    ui.fillF(24, 20, 4, 80, md.accent);
    ui.text(md.name, 42, 28, 24, C_TEXT);
    ui.text(md.code, 42, 60, 13, C_MUTED);
    Col risk = mapIndex_ == 0 ? C_ACCENT : (mapIndex_ == 1 ? C_GOLD : C_DANGER);
    ui.chamfer({284, 30, 82, 24}, 5, withAlpha(risk, 26));
    ui.chamferOutline({284, 30, 82, 24}, 5, withAlpha(risk, 170), 1.f);
    ui.text(md.risk, 325, 34, 13, risk, ALIGN_CENTER);
    ui.text("已搜 " + std::to_string(searched_) + " / " + std::to_string(md.containers) +
            "    击杀 " + std::to_string(kills_), 42, 76, 13, C_MUTED);

    // Mission clock.
    float remaining = std::max(0.f, timeLimit_ - elapsed_);
    bool urgent = remaining < 60.f;
    Col tc = urgent ? C_DANGER : C_TEXT;
    ui.panel({472, 20, 336, 82}, Col(14, 19, 13, 228), C_BORDER, 8);
    ui.text("行动剩余", 640, 27, 13, C_MUTED, ALIGN_CENTER);
    ui.textShadow(formatTime(remaining), 640, 43, 32, tc, ALIGN_CENTER, 0, 2);
    ui.bar({496, 84, 288, 4}, 1.f - clamp01(elapsed_ / std::max(1.f, timeLimit_)),
           urgent ? C_DANGER : md.accent, Col(5, 7, 5, 200), Col(0, 0, 0, 0));

    // Run stats.
    ui.panel({934, 20, 322, 80}, Col(14, 19, 13, 228), C_BORDER, 8);
    ui.text("战利品", 950, 30, 13, C_MUTED);
    ui.text(formatMoney(lootValue_), 950, 48, 24, C_GOLD);
    ui.text("负重", 1104, 30, 13, C_MUTED);
    float cap = BACKPACKS[backpack_].capacity;
    ui.text(formatMoney((i64)std::lround(lootWeight_ * 1000)) + "g / " + std::to_string((int)cap) + "kg",
            1104, 50, 14, lootWeight_ > cap * .95f ? C_DANGER : C_TEXT);

    // Vital bars.
    ui.panel({24, VIEW_H - 132, 348, 108}, Col(14, 19, 13, 232), C_BORDER, 8);
    ui.text("生命", 40, VIEW_H - 124, 14, C_MUTED);
    ui.text(std::to_string((int)std::ceil(player_.hp)) + " / 100", 356, VIEW_H - 124, 14, C_TEXT, ALIGN_RIGHT);
    Col hpCol = player_.hp > 55 ? C_ACCENT : (player_.hp > 28 ? C_GOLD : C_DANGER);
    ui.bar({40, VIEW_H - 104, 316, 10}, clamp01(player_.hp / 100.f), hpCol,
           Col(10, 8, 8, 220), C_BORDER);
    ui.text(ARMORS[armor_].name, 40, VIEW_H - 84, 13, C_MUTED);
    ui.text(std::to_string((int)std::lround(player_.armorHp)) + " / " + std::to_string((int)player_.armorMax),
            356, VIEW_H - 84, 13, player_.armorHp <= 0 ? C_DANGER : C_CYAN, ALIGN_RIGHT);
    ui.bar({40, VIEW_H - 66, 316, 5}, player_.armorMax > 0 ? clamp01(player_.armorHp / player_.armorMax) : 0.f,
           C_CYAN, Col(10, 8, 8, 220), C_BORDER);
    ui.text("体力", 40, VIEW_H - 49, 13, C_MUTED);
    ui.bar({86, VIEW_H - 47, 270, 5}, clamp01(player_.stamina / 100.f), C_GOLD,
           Col(10, 8, 8, 220), C_BORDER);

    // Weapon block.
    const WeaponDef& w = WEAPONS[weapon_];
    ui.panel({430, VIEW_H - 118, 420, 94}, Col(14, 19, 13, 234), C_BORDER, 8);
    if (knifeOut_) {
        ui.text("蛆牙短刀", 448, VIEW_H - 110, 20, C_TEXT);
        ui.text("近战", 832, VIEW_H - 107, 13, C_MUTED, ALIGN_RIGHT);
        ui.textShadow("刀", 448, VIEW_H - 88, 48, C_ACCENT, ALIGN_LEFT, 0, 3);
        ui.text("左键挥砍 · 无需弹药", 520, VIEW_H - 78, 14, C_TEXT);
        ui.text("吃药 [1] 绷带×" + std::to_string(bandages_) + "  [2] 止痛针×" + std::to_string(medkits_),
                520, VIEW_H - 56, 11, C_MUTED);
        if (w.id == WPN_NONE) {
            ui.text("跑刀模式  ·  无枪可切", 520, VIEW_H - 42, 11, C_MUTED);
        } else {
            ui.text("[V] 切回枪械  ·  [R] 换弹", 520, VIEW_H - 42, 11, C_GOLD);
        }
    } else {
        ui.text(w.name, 448, VIEW_H - 110, 20, C_TEXT);
        ui.text(ammoShort(w.ammo), 832, VIEW_H - 107, 13, C_MUTED, ALIGN_RIGHT);
        Col ammoCol = loaded_ <= 2 ? C_DANGER : (loaded_ <= w.mag / 3 ? C_GOLD : C_TEXT);
        std::string loaded = std::to_string(loaded_);
        std::string reserve = " / " + std::to_string(ammo_[w.ammo]);
        ui.textShadow(loaded, 448, VIEW_H - 84, 48, ammoCol, ALIGN_LEFT, 0, 3);
        float lw = ui.font ? ui.font->measure(48, loaded) : 70.f;
        ui.text(reserve, 448 + lw + 4, VIEW_H - 66, 24, C_MUTED);
        ui.text("[R] 换弹", 600, VIEW_H - 100, 13, C_MUTED);
        ui.text("吃药 [1] 绷带×" + std::to_string(bandages_) + "  [2] 止痛针×" + std::to_string(medkits_),
                600, VIEW_H - 78, 11, C_TEXT);
        if (player_.reloading) {
            float k = 1.f - player_.reloadT / std::max(.01f, w.reload);
            ui.bar({448, VIEW_H - 50, 384, 7}, clamp01(k), C_ACCENT, Col(10, 8, 8, 220), C_BORDER);
            ui.text("换弹中…", 640, VIEW_H - 42, 12, C_ACCENT, ALIGN_CENTER);
        } else {
            ui.bar({448, VIEW_H - 50, 384, 3}, clamp01(length(player_.vel) / 300.f), md.accent,
                   Col(10, 8, 8, 200), Col(0, 0, 0, 0));
        }
    }

    // Backpack block.
    ui.panel({870, VIEW_H - 118, 386, 94}, Col(14, 19, 13, 234), C_BORDER, 8);
    ui.text(BACKPACKS[backpack_].name, 886, VIEW_H - 110, 18, C_TEXT);
    ui.text((lootWeight_ > cap * .95f ? "超重 " : "负重 ") +
            std::to_string((int)std::lround(lootWeight_ * 10) / 10.0f) + " / " +
            std::to_string((int)cap) + " kg", 1240, VIEW_H - 108, 13,
            lootWeight_ > cap * .95f ? C_DANGER : C_MUTED, ALIGN_RIGHT);
    ui.bar({886, VIEW_H - 84, 354, 8}, clamp01(lootWeight_ / std::max(.1f, cap)),
           lootWeight_ > cap * .95f ? C_DANGER : C_GOLD, Col(10, 8, 8, 220), C_BORDER);
    ui.text("搜刮点 " + std::to_string(searched_) + " / " + std::to_string(md.containers),
            886, VIEW_H - 64, 13, C_MUTED);
    ui.text("撤离点 " + std::to_string(map_.extracts.size()) + "  ·  TAB 战术地图", 1240,
            VIEW_H - 64, 13, C_MUTED, ALIGN_RIGHT);
    ui.text("ESC 暂停  ·  F 手电  ·  SHIFT 冲刺", 886, VIEW_H - 43, 12, Col(109, 119, 95), ALIGN_LEFT);

    // Toasts.
    for (size_t i = 0; i < toasts_.size(); ++i) {
        float a = std::min(1.f, toasts_[i].life);
        float y = 112.f + i * 44.f;
        ui.chamfer({896, y, 360, 38}, 6, Col(10, 14, 9, (Uint8)(210 * a)));
        ui.fillF(896, y, 3, 38, withAlpha(toasts_[i].color, (Uint8)(200 * a)));
        ui.wrapped(toasts_[i].text, 912, y + 8, 13, withAlpha(toasts_[i].color, (Uint8)(235 * a)), 330, 1);
    }

    // Action prompt.
    if (extractIndex_ >= 0) {
        ui.panel({420, 606, 440, 62}, Col(14, 19, 13, 238), withAlpha(C_ACCENT, 160), 8);
        ui.textShadow("按住 E 撤离", 640, 615, 22, C_ACCENT, ALIGN_CENTER, 0, 2);
        ui.text(map_.extracts[(size_t)extractIndex_].name, 640, 638, 13, C_MUTED, ALIGN_CENTER);
        ui.bar({456, 656, 368, 4}, clamp01(extractProgress_), C_ACCENT, Col(8, 10, 8, 220), Col(0, 0, 0, 0));
    } else if (searchIndex_ >= 0 && searchIndex_ < (int)boxes_.size()) {
        const LootBox& b = boxes_[(size_t)searchIndex_];
        std::string label = b.items.empty() ? "已经搜空" : "按住 E 搜索 " + std::string(b.kind == 1 ? "医疗箱" : (b.kind == 2 ? "弹药箱" : (b.kind == 4 ? "蛆王残骸" : (b.kind == 3 ? "尸体" : "物资箱"))));
        ui.panel({440, 606, 400, 62}, Col(14, 19, 13, 238), withAlpha(b.items.empty() ? C_MUTED : C_GOLD, 150), 8);
        ui.textShadow(label, 640, 615, 19, b.items.empty() ? C_MUTED : C_GOLD, ALIGN_CENTER, 0, 2);
        ui.text("搜刮会把弹药和药品直接收好", 640, 638, 12, C_MUTED, ALIGN_CENTER);
        ui.bar({472, 656, 336, 4}, clamp01(searchProgress_), C_GOLD, Col(8, 10, 8, 220), Col(0, 0, 0, 0));
    } else {
        const char* hint = knifeOut_
            ? (WEAPONS[weapon_].id == WPN_NONE
                ? "WASD 移动  ·  左键挥刀  ·  跑刀模式：没有枪"
                : "WASD 移动  ·  左键挥刀  ·  V 切回枪械  ·  1/2 吃药")
            : "WASD 移动  ·  鼠标瞄准  ·  长按左键连发  ·  V 切刀  ·  1/2 吃药";
        ui.textShadow(hint, 640, VIEW_H - 140, 13,
                      Col(166, 174, 143, 190), ALIGN_CENTER, 0, 2);
    }

    // Hit direction sector.
    if (lastHitTimer_ > 0.f) {
        float a = angleOf(lastHitDir_);
        Vec2 c = player_.pos - cam;
        float k = lastHitTimer_ / 1.1f;
        for (int i = -1; i <= 1; ++i) {
            Vec2 p = c + fromAngle(a + i * .12f, 62.f);
            ui.line(c + fromAngle(a + i * .12f, 48.f), p, withAlpha(C_DANGER, (Uint8)(210 * k)), 3.f);
        }
    }
}

void Raid::drawEdgeMarkers(UI& ui, Vec2 cam) {
    for (int i = 0; i < (int)map_.extracts.size(); ++i) {
        const auto& ex = map_.extracts[(size_t)i];
        Vec2 s = ex.pos - cam;
        if (s.x > 100 && s.x < VIEW_W - 100 && s.y > 120 && s.y < VIEW_H - 160) continue;
        Vec2 c = s;
        float margin = 92.f;
        c.x = std::clamp(c.x, margin, VIEW_W - margin);
        c.y = std::clamp(c.y, 116.f, 642.f);
        float d = dist(ex.pos, player_.pos) / (float)map_.tileSize;
        Vec2 dir = normalize(s - c);
        ui.diamond(c, 6.f, withAlpha(C_ACCENT, 190));
        ui.line(c, c + dir * 18.f, withAlpha(C_ACCENT, 120), 1.5f);
        ui.text(std::to_string((int)d) + "m", c.x, c.y + 11, 11, C_MUTED, ALIGN_CENTER);
    }
}

void Raid::drawCrosshair(UI& ui, Vec2 cam) {
    (void)cam;
    if (mapOpen_ || paused_ || playerDead_ || !ui.in) return;
    Vec2 m = ui.in->mouse;
    Col c = hitMarker_ > 0 ? (critMarker_ ? C_GOLD : C_WHITE) : Col(224, 236, 200, 215);
    ui.crosshair(m, 5.f, 10.f, c, hitMarker_ > 0);
    if (hitMarker_ > 0) {
        float k = hitMarker_ / .16f;
        for (int i = 0; i < 4; ++i) {
            float a = RPI * .25f + i * RPI * .5f;
            ui.line(m + fromAngle(a, 7.f), m + fromAngle(a, 14.f), withAlpha(c, (Uint8)(220 * k)), 2.f);
        }
    }
    if (player_.reloading) {
        float k = 1.f - player_.reloadT / std::max(.01f, WEAPONS[weapon_].reload);
        ui.ring(m, 22.f, Col(0, 0, 0, 120), 3.f, 22);
        ui.ring(m, 22.f, withAlpha(C_ACCENT, 190), 2.f, 22);
        (void)k;
    }
}

void Raid::drawPauseOverlay(UI& ui) {
    ui.fillF(0, 0, VIEW_W, VIEW_H, Col(4, 7, 4, 208));
    Rect p{396, 184, 488, 410};
    ui.panel(p, Col(14, 19, 13, 248), C_BORDER, 14);
    ui.fillF(p.x, p.y, 5, p.h, C_ACCENT);
    ui.text("行动暂停", 640, 212, 48, C_TEXT, ALIGN_CENTER);
    ui.text(std::string(MAPS[mapIndex_].name) + "  ·  " + MAPS[mapIndex_].code, 640, 270, 14, C_MUTED, ALIGN_CENTER);
    ui.text("行动时间  " + formatTime(elapsed_), 432, 318, 18, C_TEXT);
    ui.text("击杀  " + std::to_string(kills_), 432, 348, 18, C_TEXT);
    ui.text("战利品  " + formatMoney(lootValue_), 432, 378, 18, C_GOLD);
    ui.text("搜刮  " + std::to_string(searched_) + " 个容器", 432, 408, 18, C_TEXT);
    ui.text("死亡会丢失所有装备、弹药与本次战利品。", 640, 454, 13, C_MUTED, ALIGN_CENTER);
    if (ui.button({432, 488, 416, 46}, "继续行动", true, true, 20)) {
        paused_ = false;
    }
    if (ui.button({432, 544, 416, 42}, "放弃行动并撤离", true, false, 16)) {
        finish(false, "主动放弃行动");
    }
    ui.text("TAB 在行动中查看战术地图", 640, 606, 12, Col(109, 119, 95), ALIGN_CENTER);
}

void Raid::drawMapOverlay(UI& ui) {
    ui.fillF(0, 0, VIEW_W, VIEW_H, Col(3, 6, 4, 232));
    Rect p{58, 34, 1164, 732};
    ui.panel(p, Col(12, 17, 11, 250), C_BORDER, 14);
    ui.fillF(p.x, p.y, 5, p.h, C_ACCENT);
    ui.text("战术地图", 88, 56, 32, C_TEXT);
    ui.text(std::string(MAPS[mapIndex_].name) + " / " + MAPS[mapIndex_].code, 88, 96, 14, C_MUTED);
    ui.text("剩余 " + formatTime(std::max(0.f, timeLimit_ - elapsed_)), 1168, 64, 24, C_TEXT, ALIGN_RIGHT);
    ui.text("视野内地形已标记，未探索区域仍为黑区", 88, 114, 12, Col(109, 119, 95));
    float areaX = 88, areaY = 140, areaW = 1084, areaH = 540;
    float s = std::min(areaW / map_.w, areaH / map_.h);
    float ox = areaX + (areaW - map_.w * s) * .5f;
    float oy = areaY + (areaH - map_.h * s) * .5f;
    ui.fillF(areaX, areaY, areaW, areaH, Col(5, 8, 5, 220));
    const MapDef& md = MAPS[mapIndex_];
    for (int y = 0; y < map_.h; ++y) {
        for (int x = 0; x < map_.w; ++x) {
            const Tile& t = map_.at(x, y);
            if (!t.seen) continue;
            Rect r{ox + x * s, oy + y * s, std::max(1.f, s - .5f), std::max(1.f, s - .5f)};
            bool v = tileVis(x, y);
            Col c;
            if (t.type == T_WALL) c = shade(md.wall, v ? 2.35f : 1.85f);
            else if (t.type == T_CRATE) c = shade(Col(112, 88, 50), v ? 1.45f : 1.1f);
            else if (t.type == T_BARREL) c = shade(Col(83, 94, 57), v ? 1.45f : 1.1f);
            else if (t.type == T_PILLAR) c = shade(Col(82, 79, 65), v ? 1.5f : 1.15f);
            else if (t.type == T_GRATE) c = shade(Col(43, 49, 41), v ? 1.35f : 1.0f);
            else c = shade(md.floor, v ? 1.45f : 1.1f);
            ui.fillF(r.x, r.y, r.w, r.h, c);
        }
    }
    for (int i = 0; i < (int)map_.extracts.size(); ++i) {
        const auto& ex = map_.extracts[(size_t)i];
        Vec2 sxy{ox + ex.pos.x / map_.tileSize * s, oy + ex.pos.y / map_.tileSize * s};
        float pulse = .5f + .5f * std::sin(animTime_ * 3.f + i);
        ui.diamond(sxy, 7.f + pulse * 2.f, C_ACCENT);
        ui.text(ex.name, sxy.x, sxy.y + 12, 13, C_ACCENT, ALIGN_CENTER);
    }
    Vec2 ps{ox + player_.pos.x / map_.tileSize * s, oy + player_.pos.y / map_.tileSize * s};
    ui.triangle(ps, 8.f, player_.angle, C_TEXT);
    ui.circle(ps, 3.f, C_ACCENT, 8);
    ui.text("你", ps.x, ps.y - 20, 13, C_TEXT, ALIGN_CENTER);
    ui.text("◆ 撤离点     ◣ 你的位置     （仅显示已探索区域）", 88, 704, 13, C_MUTED);
    if (ui.button({1020, 700, 170, 38}, "关闭地图 (TAB)", true, false, 15)) {
        mapOpen_ = false;
    }
}

void Raid::drawDeathOverlay(UI& ui) {
    float k = clamp01(1.f - deathTimer_ / 1.8f);
    ui.fillF(0, 0, VIEW_W, VIEW_H, Col(26, 3, 3, (Uint8)((70 + k * 110))));
    Rect p{404, 206, 472, 330};
    ui.panel(p, Col(17, 10, 10, 246), Col(92, 38, 31, 255), 14);
    ui.fillF(p.x, p.y, 5, p.h, C_DANGER);
    ui.textShadow("你 死 了", 640, 236, 48, C_DANGER, ALIGN_CENTER, 0, 3);
    ui.text(deathReason_, 640, 298, 16, C_TEXT, ALIGN_CENTER);
    ui.text(weapon_ == WPN_NONE ? "跑刀结束：家中装备未动，战利品留在封锁区。"
                               : "被吞噬的装备与战利品将留在封锁区。",
            640, 330, 13, C_MUTED, ALIGN_CENTER);
    ui.text("击杀 " + std::to_string(kills_) + "      搜刮 " + std::to_string(searched_) +
            "      战利品 " + formatMoney(lootValue_), 640, 374, 15, C_TEXT, ALIGN_CENTER);
    if (deathTimer_ < 1.1f) {
        if (ui.button({500, 448, 280, 44}, "返回巢穴", true, false, 18)) {
            finish(false, deathReason_);
        }
    } else {
        ui.text("正在回收意识…", 640, 456, 13, C_MUTED, ALIGN_CENTER);
    }
}
