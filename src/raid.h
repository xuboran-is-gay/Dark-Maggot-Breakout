#pragma once
// One raid: map, player, enemies, loot, bullets and the whole top-down game layer.
#include "core.h"
#include "data.h"
#include "map.h"
#include "profile.h"
#include "ui.h"

struct RaidResult {
    bool success = false;
    bool aborted = false;
    bool died = false;
    bool bossKilled = false;
    int mapIndex = 0;
    int kills = 0;
    int shots = 0;
    int hits = 0;
    int lootValue = 0;
    int containersSearched = 0;
    int ammo[AMMO_COUNT] = {0, 0, 0};
    int bandages = 0;
    int medkits = 0;
    float time = 0;
    std::string reason;
    std::vector<ItemStack> loot;
};

class Raid {
public:
    bool start(int mapIndex, u64 seed, const Profile& profile);
    void update(float dt, Input& in, Audio& audio);
    void render(UI& ui, Visual& visual, float time);
    bool finished() const { return done_; }
    const RaidResult& result() const { return result_; }
    bool paused() const { return paused_; }
    bool mapOpen() const { return mapOpen_; }
    int mapIndex() const { return mapIndex_; }
    const WorldMap& map() const { return map_; }
    void debugPrepareShot();
    int debugShots() const { return shots_; }
    bool debugKnifeOut() const { return knifeOut_; }
    void debugSetMapOpen(bool open);
    void debugSetPaused(bool paused);

private:
    struct Player {
        Vec2 pos, vel;
        float angle = 0;
        float hp = 100, maxHp = 100;
        float armorHp = 0, armorMax = 0;
        float radius = 13;
        float fireCd = 0, reloadT = 0;
        bool reloading = false;
        float stamina = 100, sprintLock = 0;
        bool lightOn = true;
        float muzzle = 0;
        float healT = 0;
        int healKind = 0;
        float anim = 0;
        float travel = 0;
        float walkPhase = 0;
        float sinceDamage = 99;
    };

    struct Enemy {
        EnemyType type = EN_SCAV;
        Vec2 pos, vel;
        Vec2 lastKnown;
        Vec2 patrol;
        Vec2 pathGoal;
        float angle = 0;
        int hp = 1, maxHp = 1;
        int armor = 0;
        bool alive = true;
        bool alert = false;
        bool enraged = false;
        float fireCd = 0, thinkCd = 0, pathCd = 0;
        float lostTimer = 0, idleTimer = 0;
        float hitFlash = 0, deathTime = 0, muzzle = 0;
        float speed = 80, vision = 400, damage = 8, interval = 1, accuracy = .7f, range = 500, radius = 11;
        float anim = 0;
        float meleeCd = 0;
        bool boss = false;
        std::vector<Vec2> path;
        size_t pathIdx = 0;
        bool pathToPlayer = false;
        float stuckTimer = 0;
    };

    struct LootBox {
        Vec2 pos;
        float radius = 18;
        int kind = 0; // 0 crate, 1 med, 2 ammo, 3 corpse, 4 boss corpse
        bool searched = false;
        bool discovered = false;
        float pulse = 0;
        std::vector<ItemStack> items;
    };

    struct Particle {
        Vec2 pos, vel;
        float life = 1, maxLife = 1;
        float size = 2;
        Col color;
        int kind = 0;
        float drag = 2;
    };
    struct Tracer {
        Vec2 a, b;
        float life = .06f, maxLife = .06f;
        Col color;
        float width = 1.3f;
    };
    struct Decal {
        Vec2 pos;
        float rx = 8, ry = 6;
        Col color;
        int kind = 0;
    };
    struct FloatText {
        Vec2 pos;
        std::string text;
        Col color;
        float life = 1, maxLife = 1;
        int px = 18;
    };
    struct Toast {
        std::string text;
        Col color;
        float life = 3;
    };

    WorldMap map_;
    Rng rng_{1};
    int mapIndex_ = 0;
    u64 seed_ = 0;
    int weapon_ = WPN_PISTOL;
    int armor_ = ARM_NONE;
    int backpack_ = 0;
    int ammo_[AMMO_COUNT] = {0, 0, 0};
    int loaded_ = 0;
    int bandages_ = 0, medkits_ = 0;
    float lootWeight_ = 0;
    int lootValue_ = 0;
    std::vector<ItemStack> loot_;
    Player player_;
    std::vector<Enemy> enemies_;
    std::vector<LootBox> boxes_;
    std::vector<Particle> particles_;
    std::vector<Tracer> tracers_;
    std::vector<Decal> decals_;
    std::vector<FloatText> floats_;
    std::vector<Toast> toasts_;
    std::vector<u8> vis_;
    Vec2 camera_;
    float elapsed_ = 0, animTime_ = 0;
    float timeLimit_ = 420;
    float visTimer_ = 0;
    float shake_ = 0;
    float damageFlash_ = 0;
    float extractProgress_ = 0;
    int extractIndex_ = -1;
    int searchIndex_ = -1;
    float searchProgress_ = 0;
    bool mapOpen_ = false;
    bool paused_ = false;
    bool autofire_ = true;
    bool knifeOut_ = false;
    bool knifeManual_ = false;
    bool done_ = false;
    bool playerDead_ = false;
    float deathTimer_ = 0;
    int kills_ = 0, shots_ = 0, hitTriggers_ = 0, searched_ = 0;
    bool bossKilled_ = false;
    bool shakeEnabled_ = true;
    bool lowHpPulse_ = false;
    float hitMarker_ = 0;
    bool critMarker_ = false;
    float lastHitTimer_ = 0;
    Vec2 lastHitDir_;
    float slimeTimer_ = 0;
    float stepTimer_ = 0;
    float alertSoundCd_ = 0;
    RaidResult result_;
    std::string deathReason_;

    void updatePlayer(float dt, Input& in, Audio& audio);
    void updateEnemies(float dt, Audio& audio);
    void updateEffects(float dt);
    void updateVisibility(float dt);
    void updateCamera(float dt);
    bool pointVisible(Vec2 p) const;
    bool tileSeen(int x, int y) const;
    bool tileVis(int x, int y) const;
    bool canSeePlayer(const Enemy& e) const;
    void fireWeapon(Input& in, Audio& audio);
    void meleeAttack(Audio& audio);
    void switchToKnife(Audio& audio, bool manual = false);
    void toggleKnife(Audio& audio);
    void startReload(Audio& audio);
    void finishReload();
    void useMed(int kind, Audio& audio);
    void damagePlayer(float dmg, Vec2 from, Audio& audio);
    void damageEnemy(Enemy& e, float dmg, bool crit, Vec2 from, Audio& audio);
    void killEnemy(Enemy& e, Audio& audio);
    void enemyShoot(Enemy& e, Audio& audio);
    void alertEnemies(Vec2 p, float radius);
    void interact(float dt, Input& in, Audio& audio);
    void lootBox(size_t idx, Audio& audio);
    void spawnEnemies();
    void spawnBoxes();
    void addBox(Vec2 p, int kind, Rng& rng);
    void dropCorpse(Enemy& e, Audio& audio);
    std::vector<ItemStack> rollLoot(int kind, Rng& rng);
    void addParticles(Vec2 p, int kind, int n, Col col, float speed, float life, float size);
    void addBlood(Vec2 p, float amount);
    void addTracer(Vec2 a, Vec2 b, Col c, float w = 1.3f);
    void toast(const std::string& text, Col color = C_TEXT);
    void floatText(Vec2 p, const std::string& text, Col color, int px = 18);
    void moveCircle(Vec2& p, Vec2 delta, float radius);
    bool circleSolid(Vec2 p, float radius) const;
    Vec2 clampCamera(Vec2 c) const;
    const Room* roomAt(Vec2 p) const;
    Vec2 randomPatrolPoint(Enemy& e);
    void finish(bool success, const std::string& reason);
    void checkPlayerDeath();
    void resetInteractions();

    void drawWorld(UI& ui, Vec2 cam);
    void drawTile(UI& ui, int x, int y, Vec2 cam);
    void drawBox(UI& ui, const LootBox& b, Vec2 cam);
    void drawEnemy(UI& ui, const Enemy& e, Vec2 cam);
    void drawPlayer(UI& ui, Vec2 cam);
    void drawMaggot(UI& ui, Vec2 p, float angle, float scale, Col body, Col dark,
                    float phase, bool boss, bool dead = false);
    void drawGun(UI& ui, Vec2 p, float angle, int weapon, float scale, Col col);
    void drawKnife(UI& ui, Vec2 p, float angle, float scale, Col col);
    void drawLighting(UI& ui, Visual& visual, Vec2 cam);
    void drawFlashlightCone(UI& ui, Vec2 origin, float angle, float range, float half, Col tint);
    void drawHUD(UI& ui, Vec2 cam);
    void drawEdgeMarkers(UI& ui, Vec2 cam);
    void drawMapOverlay(UI& ui);
    void drawPauseOverlay(UI& ui);
    void drawCrosshair(UI& ui, Vec2 cam);
    void drawDeathOverlay(UI& ui);
    void drawExtraction(UI& ui, Vec2 cam);
    void drawDecals(UI& ui, Vec2 cam);
};
