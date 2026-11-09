#pragma once
#include "core.h"
#include "data.h"
#include <queue>

enum TileType : u8 {
    T_WALL = 0, T_FLOOR, T_GRATE, T_CRATE, T_BARREL, T_PILLAR
};

struct Tile {
    u8 type = T_WALL;
    u8 variant = 0;
    u8 seen = 0;
};

struct Room {
    Rect rect;
    Vec2 center;
    int id = 0;
};

struct ExtractPoint {
    Vec2 pos;
    Rect zone;
    std::string name;
};

class WorldMap {
public:
    int w = 0, h = 0;
    int tileSize = 36;
    int mapId = 0;
    u64 seed = 0;
    std::vector<Tile> tiles;
    std::vector<Room> rooms;
    std::vector<ExtractPoint> extracts;
    std::vector<Vec2> lamps;
    Vec2 spawn;

    bool generate(int mapIndex, u64 seed);
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Tile& at(int x, int y) { return tiles[(size_t)y * w + x]; }
    const Tile& at(int x, int y) const { return tiles[(size_t)y * w + x]; }
    bool solid(int x, int y) const {
        if (!inBounds(x, y)) return true;
        u8 t = at(x, y).type;
        return t == T_WALL || t == T_CRATE || t == T_BARREL || t == T_PILLAR;
    }
    bool solidAt(Vec2 p) const { return solid((int)std::floor(p.x / tileSize), (int)std::floor(p.y / tileSize)); }
    Vec2 tileCenter(int x, int y) const { return {(x + .5f) * tileSize, (y + .5f) * tileSize}; }
    float wallRay(Vec2 a, Vec2 b) const; // returns t in [0,1] of first wall hit, 1 if clear
    bool lineOfSight(Vec2 a, Vec2 b) const;
    bool path(Vec2 from, Vec2 to, std::vector<Vec2>& out) const;
    bool reachable(Vec2 from, Vec2 to) const;
    Vec2 nearestFloor(Vec2 p) const;
    Vec2 randomFloorInRoom(Rng& rng, const Room& room, float margin = 1.2f) const;
    void reveal(Vec2 center, float radiusPx);
    void revealAll();

private:
    struct Node { int x, y; int parent; float g, f; };
    bool bspSplit(Rect area, int depth, std::vector<Rect>& leaves, Rng& rng);
    void carveRoom(Rect r);
    void carveCorridor(Vec2 a, Vec2 b, int width, Rng& rng);
    void placeProps(Rng& rng);
    void clearZone(Vec2 c, int radius);
    bool validate() const;
};

const char* tileName(int mapId, int extractIndex);
