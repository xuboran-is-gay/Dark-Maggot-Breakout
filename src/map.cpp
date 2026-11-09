#include "map.h"
#include <array>
#include <limits>

namespace {
constexpr int DX4[4] = {1, -1, 0, 0};
constexpr int DY4[4] = {0, 0, 1, -1};
constexpr int DX8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int DY8[8] = {0, 0, 1, -1, 1, -1, 1, -1};

struct BNode { Rect area; int depth; };

const char* EXTRACT_NAMES[3][3] = {
    { "西北泵站", "废弃地铁口", "排水竖井" },
    { "冷库后门", "生鲜卸货区", "员工消防梯" },
    { "焚化炉", "污水闸门", "蛆王巢侧门" },
};
} // namespace

bool WorldMap::generate(int mapIndex, u64 s) {
    mapId = mapIndex;
    seed = s;
    const MapDef& def = MAPS[mapIndex];
    for (int attempt = 0; attempt < 12; ++attempt) {
        Rng rng(s + (u64)attempt * 7919ull);
        w = def.w; h = def.h;
        tiles.assign((size_t)w * h, Tile{T_WALL, 0, 0});
        rooms.clear();
        extracts.clear();
        lamps.clear();
        std::vector<Rect> leaves;
        bspSplit(Rect(2, 2, (float)w - 4, (float)h - 4), 0, leaves, rng);
        for (const Rect& r : leaves) {
            carveRoom(r);
            rooms.push_back(Room{r, r.center() * (float)tileSize, (int)rooms.size()});
        }
        // Connect the BSP leaves through a shuffled neighboring tree: a practical
        // approximation is a nearest-neighbour chain plus a few extra links.
        std::vector<int> order(rooms.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
        for (size_t i = order.size() - 1; i > 0; --i) std::swap(order[i], order[rng.range(0, (int)i)]);
        for (size_t i = 1; i < order.size(); ++i) {
            const Room& a = rooms[(size_t)order[i - 1]];
            const Room& b = rooms[(size_t)order[i]];
            carveCorridor(a.center, b.center, (mapIndex == 0 ? 2 : 2), rng);
        }
        // A couple of loops make firefights less linear.
        int extras = 1 + mapIndex;
        for (int i = 0; i < extras && rooms.size() > 3; ++i) {
            const Room& a = rooms[(size_t)rng.range(0, (int)rooms.size() - 1)];
            const Room& b = rooms[(size_t)rng.range(0, (int)rooms.size() - 1)];
            if (&a != &b) carveCorridor(a.center, b.center, 2, rng);
        }
        // Doors / openings into corridors are naturally carved by the corridors.

        // Choose a spawn near the west side, then the farthest two rooms as exits.
        std::vector<int> sorted(rooms.size());
        for (size_t i = 0; i < sorted.size(); ++i) sorted[i] = (int)i;
        std::sort(sorted.begin(), sorted.end(), [&](int a, int b) { return rooms[(size_t)a].center.x < rooms[(size_t)b].center.x; });
        int spawnIdx = sorted[(size_t)rng.range(0, std::min<int>(2, (int)sorted.size() - 1))];
        spawn = rooms[(size_t)spawnIdx].center;
        clearZone(spawn, 2);
        std::vector<int> byDist(sorted.size());
        for (size_t i = 0; i < byDist.size(); ++i) byDist[i] = (int)i;
        std::sort(byDist.begin(), byDist.end(), [&](int a, int b) {
            return dist2(rooms[(size_t)a].center, spawn) > dist2(rooms[(size_t)b].center, spawn);
        });
        for (int idx : byDist) {
            if ((int)extracts.size() >= def.extracts) break;
            if (idx == spawnIdx) continue;
            if (dist(rooms[(size_t)idx].center, spawn) < 300.f) continue;
            bool tooClose = false;
            for (auto& e : extracts) if (dist(e.pos, rooms[(size_t)idx].center) < 360.f) tooClose = true;
            if (tooClose) continue;
            clearZone(rooms[(size_t)idx].center, 2);
            ExtractPoint ep;
            ep.pos = rooms[(size_t)idx].center;
            ep.zone = Rect(ep.pos.x - tileSize * 1.7f, ep.pos.y - tileSize * 1.7f, tileSize * 3.4f, tileSize * 3.4f);
            int nameIdx = (int)extracts.size();
            ep.name = EXTRACT_NAMES[mapIndex][std::min(2, nameIdx + rng.range(0, 0))];
            extracts.push_back(ep);
        }
        if ((int)extracts.size() < def.extracts) continue;
        // Make sure the names differ.
        if (extracts.size() >= 2 && extracts[0].name == extracts[1].name) extracts[1].name = EXTRACT_NAMES[mapIndex][2];

        placeProps(rng);
        // Props might have landed on extraction / spawn pads; clear them again.
        clearZone(spawn, 2);
        for (auto& e : extracts) clearZone(e.pos, 2);
        for (int i = 0; i < 20; ++i) {
            Vec2 p{rng.rangeF(tileSize * 2.f, (w - 2) * (float)tileSize), rng.rangeF(tileSize * 2.f, (h - 2) * (float)tileSize)};
            if (!solidAt(p)) lamps.push_back(p);
        }
        if (validate()) return true;
    }
    return false;
}

bool WorldMap::bspSplit(Rect area, int depth, std::vector<Rect>& leaves, Rng& rng) {
    const float minW = 10, minH = 8;
    bool canH = area.w > minW * 2, canV = area.h > minH * 2;
    if (depth >= 4 || (!canH && !canV)) {
        float w = rng.rangeF(std::max(9.f, area.w * .42f), std::max(10.f, area.w * .82f));
        float h = rng.rangeF(std::max(7.f, area.h * .42f), std::max(8.f, area.h * .82f));
        w = std::min(w, area.w - 2); h = std::min(h, area.h - 2);
        float x = area.x + rng.rangeF(1.f, std::max(1.f, area.w - w - 1));
        float y = area.y + rng.rangeF(1.f, std::max(1.f, area.h - h - 1));
        leaves.push_back(Rect(std::floor(x), std::floor(y), std::floor(w), std::floor(h)));
        return true;
    }
    bool splitH = canH && (!canV || area.w > area.h * 1.05f || (std::abs(area.w - area.h) < 2 && rng.chance(.5f)));
    float cut = splitH ? area.x + area.w * rng.rangeF(.38f, .62f) : area.y + area.h * rng.rangeF(.38f, .62f);
    if (splitH) {
        bspSplit(Rect(area.x, area.y, cut - area.x, area.h), depth + 1, leaves, rng);
        bspSplit(Rect(cut, area.y, area.right() - cut, area.h), depth + 1, leaves, rng);
    } else {
        bspSplit(Rect(area.x, area.y, area.w, cut - area.y), depth + 1, leaves, rng);
        bspSplit(Rect(area.x, cut, area.w, area.bottom() - cut), depth + 1, leaves, rng);
    }
    return true;
}

void WorldMap::carveRoom(Rect r) {
    int x0 = std::max(1, (int)r.x), y0 = std::max(1, (int)r.y);
    int x1 = std::min(w - 2, (int)r.right() - 1), y1 = std::min(h - 2, (int)r.bottom() - 1);
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        Tile& t = at(x, y);
        t.type = T_FLOOR;
        t.variant = (u8)(hash2(x, y, (u32)seed) & 0xFF);
    }
}

void WorldMap::carveCorridor(Vec2 a, Vec2 b, int width, Rng& rng) {
    int ax = (int)(a.x / tileSize), ay = (int)(a.y / tileSize);
    int bx = (int)(b.x / tileSize), by = (int)(b.y / tileSize);
    bool horizontalFirst = rng.chance(.5f);
    auto carveH = [&](int x0, int x1, int y) {
        if (x1 < x0) std::swap(x0, x1);
        for (int x = x0; x <= x1; ++x) for (int oy = 0; oy < width; ++oy) {
            int yy = y + oy;
            if (x > 0 && x < w - 1 && yy > 0 && yy < h - 1) {
                at(x, yy).type = T_FLOOR;
                at(x, yy).variant = (u8)(hash2(x, yy, (u32)seed + 9) & 0xFF);
            }
        }
    };
    auto carveV = [&](int y0, int y1, int x) {
        if (y1 < y0) std::swap(y0, y1);
        for (int y = y0; y <= y1; ++y) for (int ox = 0; ox < width; ++ox) {
            int xx = x + ox;
            if (xx > 0 && xx < w - 1 && y > 0 && y < h - 1) {
                at(xx, y).type = T_FLOOR;
                at(xx, y).variant = (u8)(hash2(xx, y, (u32)seed + 9) & 0xFF);
            }
        }
    };
    if (horizontalFirst) { carveH(ax, bx, ay); carveV(ay, by, bx); }
    else { carveV(ay, by, ax); carveH(ax, bx, by); }
}

void WorldMap::clearZone(Vec2 c, int radius) {
    int cx = (int)(c.x / tileSize), cy = (int)(c.y / tileSize);
    for (int y = cy - radius; y <= cy + radius; ++y) for (int x = cx - radius; x <= cx + radius; ++x) {
        if (inBounds(x, y) && std::abs(x - cx) + std::abs(y - cy) <= radius + 1) {
            at(x, y).type = T_FLOOR;
        }
    }
    // Keep a two-tile clear approach around pads.
    for (int y = cy - radius; y <= cy + radius; ++y) for (int x = cx - radius; x <= cx + radius; ++x) {
        if (inBounds(x, y) && solid(x, y)) at(x, y).type = T_WALL;
    }
}

void WorldMap::placeProps(Rng& rng) {
    // Cover in rooms: crates and barrels, but never on the room centreline.
    for (const Room& room : rooms) {
        int count = rng.range(0, mapId == 2 ? 3 : 2);
        for (int i = 0; i < count; ++i) {
            int x = (int)room.rect.x + rng.range(1, std::max(1, (int)room.rect.w - 2));
            int y = (int)room.rect.y + rng.range(1, std::max(1, (int)room.rect.h - 2));
            if (!inBounds(x, y) || at(x, y).type != T_FLOOR) continue;
            if (dist(tileCenter(x, y), room.center) < tileSize * 1.7f) continue;
            at(x, y).type = rng.chance(.55f) ? T_CRATE : T_BARREL;
        }
    }
    // Occasional pillars in the large industrial rooms.
    if (mapId >= 1) {
        for (const Room& room : rooms) {
            if (room.rect.w < 8 || room.rect.h < 7) continue;
            int x = (int)room.center.x / tileSize + rng.range(-3, 3);
            int y = (int)room.center.y / tileSize + rng.range(-3, 3);
            if (inBounds(x, y) && at(x, y).type == T_FLOOR) at(x, y).type = T_PILLAR;
        }
    }
    // Grate / wet floor decoration near the edges.
    for (int y = 1; y < h - 1; ++y) for (int x = 1; x < w - 1; ++x) {
        if (at(x, y).type == T_FLOOR && hash01(x, y, (u32)seed + 77) < .06f) at(x, y).type = T_GRATE;
    }
}

bool WorldMap::validate() const {
    // Flood fill from spawn and check every extract is reachable.
    std::vector<u8> vis((size_t)w * h, 0);
    int sx = (int)(spawn.x / tileSize), sy = (int)(spawn.y / tileSize);
    if (solid(sx, sy)) return false;
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy}); vis[(size_t)sy * w + sx] = 1;
    int seen = 0;
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop(); ++seen;
        for (int d = 0; d < 4; ++d) {
            int nx = x + DX4[d], ny = y + DY4[d];
            if (!inBounds(nx, ny) || vis[(size_t)ny * w + nx] || solid(nx, ny)) continue;
            vis[(size_t)ny * w + nx] = 1;
            q.push({nx, ny});
        }
    }
    for (auto& e : extracts) {
        int ex = (int)(e.pos.x / tileSize), ey = (int)(e.pos.y / tileSize);
        if (!vis[(size_t)ey * w + ex]) return false;
    }
    return seen > (int)(w * h * .12f);
}

float WorldMap::wallRay(Vec2 a, Vec2 b) const {
    Vec2 d = b - a;
    float distT = length(d);
    if (distT < .0001f) return 1.f;
    Vec2 dir = d / distT;
    int tx = (int)std::floor(a.x / tileSize), ty = (int)std::floor(a.y / tileSize);
    int stepX = dir.x > 0 ? 1 : -1, stepY = dir.y > 0 ? 1 : -1;
    float tDeltaX = std::abs(tileSize / (dir.x * distT + 1e-9f));
    float tDeltaY = std::abs(tileSize / (dir.y * distT + 1e-9f));
    float nextX = (dir.x > 0 ? (tx + 1) * tileSize - a.x : a.x - tx * tileSize) / std::abs(dir.x * distT + 1e-9f);
    float nextY = (dir.y > 0 ? (ty + 1) * tileSize - a.y : a.y - ty * tileSize) / std::abs(dir.y * distT + 1e-9f);
    float tMaxX = dir.x == 0 ? std::numeric_limits<float>::infinity() : std::abs(nextX / distT);
    float tMaxY = dir.y == 0 ? std::numeric_limits<float>::infinity() : std::abs(nextY / distT);
    if (solid(tx, ty)) return 0.f;
    for (int i = 0; i < w + h + 4; ++i) {
        if (tMaxX < tMaxY) { tx += stepX; if (tMaxX > 1.f) return 1.f; if (solid(tx, ty)) return std::min(1.f, tMaxX); tMaxX += tDeltaX; }
        else { ty += stepY; if (tMaxY > 1.f) return 1.f; if (solid(tx, ty)) return std::min(1.f, tMaxY); tMaxY += tDeltaY; }
    }
    return 1.f;
}

bool WorldMap::lineOfSight(Vec2 a, Vec2 b) const {
    return wallRay(a, b) >= .999f;
}

Vec2 WorldMap::nearestFloor(Vec2 p) const {
    int cx = (int)(p.x / tileSize), cy = (int)(p.y / tileSize);
    if (!solid(cx, cy)) return p;
    for (int r = 1; r < 12; ++r) {
        for (int y = cy - r; y <= cy + r; ++y) for (int x = cx - r; x <= cx + r; ++x) {
            if (!solid(x, y)) return tileCenter(x, y);
        }
    }
    return p;
}

bool WorldMap::path(Vec2 from, Vec2 to, std::vector<Vec2>& out) const {
    out.clear();
    int sx = std::clamp((int)(from.x / tileSize), 0, w - 1), sy = std::clamp((int)(from.y / tileSize), 0, h - 1);
    int tx = std::clamp((int)(to.x / tileSize), 0, w - 1), ty = std::clamp((int)(to.y / tileSize), 0, h - 1);
    if (solid(sx, sy)) { Vec2 nf = nearestFloor(from); sx = (int)(nf.x / tileSize); sy = (int)(nf.y / tileSize); }
    if (solid(tx, ty)) { Vec2 nf = nearestFloor(to); tx = (int)(nf.x / tileSize); ty = (int)(nf.y / tileSize); }
    if (solid(sx, sy) || solid(tx, ty)) return false;
    if (sx == tx && sy == ty) { out.push_back(to); return true; }
    const int N = w * h;
    std::vector<float> g((size_t)N, std::numeric_limits<float>::infinity());
    std::vector<int> prev((size_t)N, -1);
    std::vector<u8> closed((size_t)N, 0);
    struct QN { float f; int idx; bool operator>(const QN& o) const { return f > o.f; } };
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> open;
    int start = sy * w + sx, goal = ty * w + tx;
    auto heuristic = [&](int x, int y) { return std::sqrt((float)((x - tx) * (x - tx) + (y - ty) * (y - ty))); };
    g[(size_t)start] = 0;
    open.push({heuristic(sx, sy), start});
    bool found = false;
    while (!open.empty()) {
        QN cur = open.top(); open.pop();
        int cx = cur.idx % w, cy = cur.idx / w;
        if (closed[(size_t)cur.idx]) continue;
        closed[(size_t)cur.idx] = 1;
        if (cur.idx == goal) { found = true; break; }
        for (int d = 0; d < 8; ++d) {
            int nx = cx + DX8[d], ny = cy + DY8[d];
            if (!inBounds(nx, ny) || solid(nx, ny)) continue;
            if (d >= 4 && (solid(cx + DX8[d], cy) || solid(cx, cy + DY8[d]))) continue;
            float cost = (d >= 4 ? 1.41421356f : 1.f);
            float ng = g[(size_t)cur.idx] + cost;
            int ni = ny * w + nx;
            if (ng + .0001f < g[(size_t)ni]) {
                g[(size_t)ni] = ng;
                prev[(size_t)ni] = cur.idx;
                open.push({ng + heuristic(nx, ny), ni});
            }
        }
    }
    if (!found) return false;
    std::vector<Vec2> raw;
    int cur = goal;
    while (cur != -1) {
        raw.push_back(tileCenter(cur % w, cur / w));
        if (cur == start) break;
        cur = prev[(size_t)cur];
    }
    std::reverse(raw.begin(), raw.end());
    if (!raw.empty()) raw.back() = nearestFloor(to);
    // String-pull using line of sight.
    out.push_back(raw.front());
    size_t i = 0;
    while (i + 1 < raw.size()) {
        size_t best = i + 1;
        for (size_t j = raw.size() - 1; j > i + 1; --j) {
            if (lineOfSight(raw[i], raw[j])) { best = j; break; }
        }
        out.push_back(raw[best]);
        i = best;
    }
    if (out.size() > 1) out.erase(out.begin());
    return !out.empty();
}

bool WorldMap::reachable(Vec2 from, Vec2 to) const {
    std::vector<Vec2> p;
    return path(from, to, p);
}

Vec2 WorldMap::randomFloorInRoom(Rng& rng, const Room& room, float margin) const {
    for (int tries = 0; tries < 32; ++tries) {
        int x = (int)room.rect.x + rng.range(1, std::max(1, (int)room.rect.w - 2));
        int y = (int)room.rect.y + rng.range(1, std::max(1, (int)room.rect.h - 2));
        if (!solid(x, y) && dist(tileCenter(x, y), room.center) < (room.rect.w + room.rect.h) * tileSize * .5f) {
            Vec2 p = tileCenter(x, y);
            if (margin <= 0 || dist(p, room.center) > margin * tileSize) return p;
        }
    }
    return room.center;
}

void WorldMap::reveal(Vec2 center, float radiusPx) {
    int cx = (int)(center.x / tileSize), cy = (int)(center.y / tileSize);
    int r = (int)std::ceil(radiusPx / tileSize);
    for (int y = cy - r; y <= cy + r; ++y) for (int x = cx - r; x <= cx + r; ++x) {
        if (!inBounds(x, y)) continue;
        Vec2 p = tileCenter(x, y);
        if (dist2(p, center) > radiusPx * radiusPx) continue;
        if (lineOfSight(center, p)) at(x, y).seen = 1;
    }
}

void WorldMap::revealAll() {
    for (auto& t : tiles) t.seen = 1;
}

const char* tileName(int mapId, int extractIndex) {
    return EXTRACT_NAMES[std::clamp(mapId, 0, 2)][std::clamp(extractIndex, 0, 2)];
}
