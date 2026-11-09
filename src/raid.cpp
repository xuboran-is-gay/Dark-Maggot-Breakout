#include "raid.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr float PI = 3.14159265f;
constexpr float TAU = 6.2831853f;

float angDiff(float a, float b) {
    float d = std::fmod(a - b + PI, TAU);
    if (d < 0) d += TAU;
    return d - PI;
}
float approachAngle(float cur, float target, float step) {
    float d = angDiff(target, cur);
    if (std::fabs(d) <= step) return target;
    return cur + (d > 0 ? step : -step);
}
Sfx weaponSfx(int w) {
    switch (w) {
        case WPN_SMG: return SFX_SHOT_SMG;
        case WPN_SHOTGUN: return SFX_SHOT_SHOTGUN;
        case WPN_RIFLE: return SFX_SHOT_RIFLE;
        default: return SFX_SHOT_PISTOL;
    }
}
Sfx enemySfx(EnemyType t) {
    switch (t) {
        case EN_MERC: return SFX_SHOT_SMG;
        case EN_BRUTE: return SFX_SHOT_SHOTGUN;
        case EN_KING: return SFX_SHOT_SHOTGUN;
        default: return SFX_SHOT_PISTOL;
    }
}
const char* boxName(int kind) {
    switch (kind) {
        case 1: return "医疗箱";
        case 2: return "弹药箱";
        case 3: return "尸体";
        case 4: return "蛆王残骸";
        default: return "物资箱";
    }
}
int ammoItemType(int id) {
    if (id == IT_AMMO9) return AMMO_9MM;
    if (id == IT_AMMO12G) return AMMO_12G;
    if (id == IT_AMMO556) return AMMO_556;
    return -1;
}
void stackAdd(std::vector<ItemStack>& v, int id, int n) {
    if (n <= 0) return;
    for (auto& s : v) if (s.id == id) { s.count += n; return; }
    v.push_back({id, n});
}
} // namespace

bool Raid::start(int mapIndex, u64 seed, const Profile& profile) {
    if (mapIndex < 0 || mapIndex >= 3) return false;
    mapIndex_ = mapIndex;
    seed_ = seed ? seed : 0xD16B00B5ull;
    rng_.reseed(seed_ ^ (u64)(mapIndex + 1) * 0x9E3779B97F4A7C15ull);
    if (!map_.generate(mapIndex, seed_)) return false;

    // Reset all per-raid state.
    done_ = false;
    playerDead_ = false;
    paused_ = false;
    mapOpen_ = false;
    deathTimer_ = 0;
    elapsed_ = 0;
    animTime_ = 0;
    visTimer_ = 0;
    shake_ = 0;
    damageFlash_ = 0;
    extractProgress_ = 0;
    extractIndex_ = -1;
    searchIndex_ = -1;
    searchProgress_ = 0;
    kills_ = shots_ = hitTriggers_ = searched_ = 0;
    bossKilled_ = false;
    hitMarker_ = 0;
    critMarker_ = false;
    lastHitTimer_ = 0;
    alertSoundCd_ = 0;
    lootWeight_ = 0;
    lootValue_ = 0;
    loot_.clear();
    boxes_.clear();
    enemies_.clear();
    particles_.clear();
    tracers_.clear();
    decals_.clear();
    floats_.clear();
    toasts_.clear();
    result_ = RaidResult{};
    result_.mapIndex = mapIndex;
    deathReason_.clear();

    weapon_ = profile.weapon;
    if (weapon_ <= WPN_NONE || weapon_ >= WPN_COUNT) weapon_ = WPN_PISTOL;
    armor_ = profile.armor;
    if (armor_ < 0 || armor_ >= ARM_COUNT) armor_ = ARM_NONE;
    backpack_ = std::clamp(profile.backpack, 0, 3);
    for (int i = 0; i < AMMO_COUNT; ++i) ammo_[i] = std::max(0, profile.ammo[i]);
    bandages_ = std::max(0, profile.bandages);
    medkits_ = std::max(0, profile.medkits);
    const WeaponDef& w = WEAPONS[weapon_];
    loaded_ = std::min(w.mag, ammo_[w.ammo]);
    ammo_[w.ammo] -= loaded_;
    shakeEnabled_ = profile.settings.shake;
    autofire_ = profile.settings.autofire;
    knifeOut_ = loaded_ <= 0 && ammo_[w.ammo] <= 0;
    knifeManual_ = false;

    player_ = Player{};
    player_.pos = map_.spawn;
    player_.maxHp = 100;
    player_.hp = 100;
    player_.armorMax = (float)ARMORS[armor_].armor;
    player_.armorHp = player_.armorMax;
    player_.angle = 0;
    player_.lightOn = true;
    player_.walkPhase = rng_.rangeF(0, TAU);
    timeLimit_ = (float)MAPS[mapIndex_].timer;

    spawnEnemies();
    spawnBoxes();

    // Extraction intel is known before entering; reveal just the pads.
    for (auto& e : map_.extracts) map_.reveal(e.pos, 90.f);

    vis_.assign((size_t)map_.w * map_.h, 0);
    camera_ = clampCamera(player_.pos - Vec2(VIEW_W * .5f, VIEW_H * .5f));
    updateVisibility(.01f);
    updateCamera(1.f);
    toast(std::string("已潜入 ") + MAPS[mapIndex_].name, C_ACCENT);
    if (mapIndex_ == 2) toast("检测到高浓度腐化反应：蛆王在附近", C_DANGER);
    return true;
}

void Raid::spawnEnemies() {
    const MapDef& def = MAPS[mapIndex_];
    std::vector<int> candidates;
    for (int i = 0; i < (int)map_.rooms.size(); ++i) {
        if (dist(map_.rooms[(size_t)i].center, map_.spawn) > 260.f) candidates.push_back(i);
    }
    if (candidates.empty()) {
        for (int i = 0; i < (int)map_.rooms.size(); ++i) candidates.push_back(i);
    }
    auto makeEnemy = [&](EnemyType type, Vec2 p) {
        Enemy e;
        e.type = type;
        e.pos = p;
        e.angle = rng_.rangeF(0, TAU);
        const EnemyDef& d = ENEMIES[(int)type];
        e.maxHp = e.hp = d.hp;
        e.armor = d.armor;
        e.speed = d.speed;
        e.vision = d.vision;
        e.damage = d.damage;
        e.interval = d.interval;
        e.accuracy = d.accuracy;
        e.range = d.range;
        e.radius = d.radius;
        e.boss = d.boss;
        e.patrol = p;
        e.lastKnown = p;
        e.idleTimer = rng_.rangeF(.5f, 4.f);
        e.thinkCd = rng_.rangeF(0, .3f);
        e.pathCd = rng_.rangeF(0, .4f);
        enemies_.push_back(e);
    };

    if (mapIndex_ == 2) {
        // The king waits in the room furthest from the entrance, with two guards.
        int far = 0;
        float best = -1;
        for (int i = 0; i < (int)map_.rooms.size(); ++i) {
            float d = dist2(map_.rooms[(size_t)i].center, map_.spawn);
            if (d > best) { best = d; far = i; }
        }
        Vec2 bossPos = map_.nearestFloor(map_.rooms[(size_t)far].center);
        makeEnemy(EN_KING, bossPos);
        for (int g = 0; g < 2; ++g) {
            for (int tries = 0; tries < 30; ++tries) {
                Vec2 off = fromAngle(rng_.rangeF(0, TAU), rng_.rangeF(80.f, 150.f));
                Vec2 p = map_.nearestFloor(bossPos + off);
                if (!map_.solidAt(p) && dist(p, bossPos) > 55.f && dist(p, player_.pos) > 250.f) {
                    makeEnemy(g == 0 ? EN_MERC : EN_BRUTE, p);
                    break;
                }
            }
        }
    }

    while ((int)enemies_.size() < def.enemies) {
        bool placed = false;
        for (int tries = 0; tries < 60 && !placed; ++tries) {
            const Room& room = map_.rooms[(size_t)candidates[(size_t)rng_.range(0, (int)candidates.size() - 1)]];
            Vec2 p = map_.randomFloorInRoom(rng_, room, .7f);
            if (map_.solidAt(p)) continue;
            if (dist(p, player_.pos) < 360.f) continue;
            bool close = false;
            for (auto& e : enemies_) if (dist(e.pos, p) < 38.f) close = true;
            if (close) continue;
            float roll = rng_.f01();
            EnemyType type = EN_SCAV;
            if (mapIndex_ == 0) type = roll < .72f ? EN_SCAV : (roll < .95f ? EN_MERC : EN_BRUTE);
            else if (mapIndex_ == 1) type = roll < .45f ? EN_SCAV : (roll < .84f ? EN_MERC : EN_BRUTE);
            else type = roll < .28f ? EN_SCAV : (roll < .76f ? EN_MERC : EN_BRUTE);
            makeEnemy(type, p);
            placed = true;
        }
        if (!placed) break;
    }
    for (auto& e : enemies_) {
        std::vector<Vec2> p;
        if (map_.path(e.pos, e.patrol, p)) { e.path = p; e.pathIdx = 0; e.pathGoal = e.patrol; }
        e.angle = rng_.rangeF(0, TAU);
    }
}

void Raid::spawnBoxes() {
    const MapDef& def = MAPS[mapIndex_];
    int needed = def.containers;
    int made = 0;
    for (int i = 0; i < needed; ++i) {
        int kind = i == 0 ? 2 : (i == 1 ? 1 : (rng_.chance(.16f) ? 1 : (rng_.chance(.18f) ? 2 : 0)));
        bool placed = false;
        for (int tries = 0; tries < 80 && !placed; ++tries) {
            const Room& room = map_.rooms[(size_t)rng_.range(0, (int)map_.rooms.size() - 1)];
            Vec2 p = map_.randomFloorInRoom(rng_, room, 1.1f);
            if (map_.solidAt(p)) continue;
            if (dist(p, player_.pos) < 120.f) continue;
            bool close = false;
            for (auto& b : boxes_) if (dist(b.pos, p) < 58.f) close = true;
            for (auto& e : map_.extracts) if (dist(e.pos, p) < 55.f) close = true;
            if (close) continue;
            addBox(p, kind, rng_);
            placed = true;
            ++made;
        }
    }
    (void)made;
}

void Raid::addBox(Vec2 p, int kind, Rng& rng) {
    LootBox b;
    b.pos = p;
    b.kind = kind;
    b.radius = kind == 4 ? 30.f : (kind == 3 ? 20.f : 18.f);
    b.pulse = rng.rangeF(0, TAU);
    b.items = rollLoot(kind, rng);
    boxes_.push_back(std::move(b));
}

std::vector<ItemStack> Raid::rollLoot(int kind, Rng& rng) {
    std::vector<ItemStack> out;
    auto add = [&](int id, int n) { stackAdd(out, id, n); };
    auto randomAmmo = [&]() {
        float r = rng.f01();
        if (mapIndex_ == 2) return r < .58f ? IT_AMMO556 : (r < .82f ? IT_AMMO9 : IT_AMMO12G);
        return r < .62f ? IT_AMMO9 : (r < .86f ? IT_AMMO12G : IT_AMMO556);
    };
    if (kind == 0) {
        int entries = rng.range(2, 4);
        for (int i = 0; i < entries; ++i) {
            float r = rng.f01();
            if (r < .17f) add(randomAmmo(), rng.range(8, 24));
            else if (r < .28f) add(rng.chance(.7f) ? IT_BANDAGE : IT_MEDKIT, rng.range(1, 2));
            else if (r < .39f) {
                add(rng.pick(std::vector<int>{IT_SCREW, IT_BREAD, IT_OIL, IT_HAIR, IT_EYE}), rng.range(1, 2));
            } else if (r < .58f) {
                add(rng.pick(std::vector<int>{IT_PHONE, IT_MUSHROOM, IT_DOGTAG}), 1);
            } else if (r < .70f) {
                add(rng.pick(std::vector<int>{IT_PROTEIN, IT_CHIP}), 1);
            } else if (r < .74f) {
                add(rng.pick(std::vector<int>{IT_EGG, IT_GLAND}), 1);
            } else {
                add(rng.pick(std::vector<int>{IT_SCREW, IT_BREAD, IT_OIL, IT_PHONE, IT_HAIR}), 1);
            }
        }
    } else if (kind == 1) {
        add(IT_BANDAGE, rng.range(2, 3));
        if (rng.chance(.62f)) add(IT_MEDKIT, 1);
        if (rng.chance(.45f)) add(IT_PROTEIN, 1);
    } else if (kind == 2) {
        add(randomAmmo(), rng.range(16, 38));
        if (rng.chance(.55f)) add(randomAmmo(), rng.range(8, 20));
        if (rng.chance(.2f)) add(IT_BANDAGE, 1);
    } else if (kind == 3) {
        add(randomAmmo(), rng.range(6, 18));
        int entries = rng.range(1, 2);
        for (int i = 0; i < entries; ++i) {
            float r = rng.f01();
            if (r < .35f) add(rng.pick(std::vector<int>{IT_SCREW, IT_BREAD, IT_HAIR}), 1);
            else if (r < .68f) add(rng.pick(std::vector<int>{IT_PHONE, IT_MUSHROOM, IT_EYE, IT_DOGTAG}), 1);
            else add(rng.pick(std::vector<int>{IT_PROTEIN, IT_CHIP}), 1);
        }
    } else {
        add(IT_GLAND, 1);
        add(randomAmmo(), rng.range(20, 40));
        add(rng.pick(std::vector<int>{IT_EGG, IT_CHIP, IT_PROTEIN}), 1);
        add(rng.pick(std::vector<int>{IT_DOGTAG, IT_MUSHROOM, IT_PHONE}), 1);
    }
    if (out.empty()) add(IT_SCREW, 1);
    return out;
}

void Raid::update(float dt, Input& in, Audio& audio) {
    if (done_) return;
    dt = std::min(dt, .05f);
    animTime_ += dt;
    alertSoundCd_ -= dt;
    shake_ = std::max(0.f, shake_ - dt * 26.f);
    damageFlash_ = std::max(0.f, damageFlash_ - dt * 2.8f);
    hitMarker_ = std::max(0.f, hitMarker_ - dt);
    lastHitTimer_ = std::max(0.f, lastHitTimer_ - dt);

    if (in.keyHit(SDL_SCANCODE_ESCAPE)) {
        if (mapOpen_) mapOpen_ = false;
        else if (!playerDead_) paused_ = !paused_;
    }
    if (in.keyHit(SDL_SCANCODE_TAB) && !playerDead_ && !paused_) {
        mapOpen_ = !mapOpen_;
        audio.play(SFX_UI_CLICK, .6f);
    }
    if (paused_ || mapOpen_) return;

    if (playerDead_) {
        updateEffects(dt);
        updateCamera(dt);
        deathTimer_ -= dt;
        if (deathTimer_ <= 0.f) finish(false, deathReason_);
        return;
    }

    elapsed_ += dt;
    if (elapsed_ >= timeLimit_) {
        finish(false, "撤离窗口关闭：你被留在了封锁区");
        return;
    }

    updatePlayer(dt, in, audio);
    if (!playerDead_) updateEnemies(dt, audio);
    updateEffects(dt);
    if (!playerDead_) interact(dt, in, audio);
    updateVisibility(dt);
    updateCamera(dt);
    checkPlayerDeath();
}

void Raid::updatePlayer(float dt, Input& in, Audio& audio) {
    player_.anim += dt;
    player_.fireCd = std::max(0.f, player_.fireCd - dt);
    player_.muzzle = std::max(0.f, player_.muzzle - dt);
    player_.sinceDamage += dt;
    player_.sprintLock = std::max(0.f, player_.sprintLock - dt);

    if (player_.reloading) {
        player_.reloadT -= dt;
        if (player_.reloadT <= 0) {
            player_.reloading = false;
            finishReload();
        }
    }
    if (player_.healT > 0) {
        player_.healT -= dt;
        if (player_.healT <= 0) {
            if (player_.healKind == 1) {
                player_.hp = std::min(player_.maxHp, player_.hp + 24.f);
                floatText(player_.pos + Vec2(0, -30), "+24", C_ACCENT, 20);
            } else if (player_.healKind == 2) {
                player_.hp = std::min(player_.maxHp, player_.hp + 52.f);
                floatText(player_.pos + Vec2(0, -30), "+52", C_ACCENT, 22);
            }
            audio.play(SFX_HEAL, .7f);
            player_.healKind = 0;
        }
    }

    // Aim follows the mouse in world space.
    Vec2 aim = in.mouse + camera_;
    if (length(aim - player_.pos) > 4.f) player_.angle = angleOf(aim - player_.pos);

    if (in.keyHit(SDL_SCANCODE_V)) toggleKnife(audio);
    if (in.keyHit(SDL_SCANCODE_R) && !player_.reloading) startReload(audio);
    if (in.keyHit(SDL_SCANCODE_1)) useMed(1, audio);
    if (in.keyHit(SDL_SCANCODE_2)) useMed(2, audio);
    if (in.keyHit(SDL_SCANCODE_F)) {
        player_.lightOn = !player_.lightOn;
        audio.play(SFX_UI_CLICK, .5f);
        toast(player_.lightOn ? "战术手电 已开启" : "战术手电 已关闭", C_MUTED);
    }

    Vec2 mv(0, 0);
    if (in.key(SDL_SCANCODE_W) || in.key(SDL_SCANCODE_UP)) mv.y -= 1;
    if (in.key(SDL_SCANCODE_S) || in.key(SDL_SCANCODE_DOWN)) mv.y += 1;
    if (in.key(SDL_SCANCODE_A) || in.key(SDL_SCANCODE_LEFT)) mv.x -= 1;
    if (in.key(SDL_SCANCODE_D) || in.key(SDL_SCANCODE_RIGHT)) mv.x += 1;
    float ml = length(mv);
    if (ml > .01f) mv = mv / ml;

    bool wantSprint = in.key(SDL_SCANCODE_LSHIFT) && ml > .01f && player_.healT <= 0 && !player_.reloading;
    bool sprinting = wantSprint && player_.sprintLock <= 0 && player_.stamina > 2.f;
    if (sprinting) {
        player_.stamina -= 30.f * dt;
        if (player_.stamina <= 0) {
            player_.stamina = 0;
            player_.sprintLock = 1.8f;
            sprinting = false;
        }
    } else {
        player_.stamina = std::min(100.f, player_.stamina + 19.f * dt);
    }

    float speed = 196.f;
    if (sprinting) speed = 302.f;
    if (player_.reloading) speed *= .78f;
    if (player_.healT > 0) speed *= .64f;
    float cap = BACKPACKS[backpack_].capacity;
    if (lootWeight_ > cap * .88f) speed *= .9f;

    Vec2 old = player_.pos;
    if (ml > .01f) moveCircle(player_.pos, mv * speed * dt, player_.radius);
    player_.vel = (player_.pos - old) / std::max(dt, .0001f);
    float spd = length(player_.vel);
    player_.walkPhase += spd * dt * .055f;
    player_.travel += spd * dt;

    // A trail of slime, because the protagonist is, after all, a maggot.
    slimeTimer_ -= dt;
    if (slimeTimer_ <= 0 && spd > 30.f) {
        Decal d;
        d.pos = player_.pos - normalize(player_.vel) * 8.f + Vec2(rng_.rangeF(-5, 5), rng_.rangeF(-5, 5));
        d.rx = rng_.rangeF(3.5f, 6.5f);
        d.ry = rng_.rangeF(2.f, 3.8f);
        d.color = Col(87, 112, 46, rng_.range(28, 54));
        d.kind = 2;
        decals_.push_back(d);
        slimeTimer_ = sprinting ? .10f : .19f;
        if (decals_.size() > 260) decals_.erase(decals_.begin(), decals_.begin() + 40);
    }

    bool trigger = (autofire_ ? (in.down || in.pressed) : in.pressed) &&
                   player_.healT <= 0 && !player_.reloading;
    if (trigger) {
        if (player_.fireCd <= 0) {
            if (knifeOut_) {
                meleeAttack(audio);
            } else if (loaded_ <= 0) {
                const WeaponDef& w = WEAPONS[weapon_];
                if (ammo_[w.ammo] > 0) startReload(audio);
                else {
                    switchToKnife(audio);
                    meleeAttack(audio);
                }
            } else {
                fireWeapon(in, audio);
            }
        }
    }
}

void Raid::startReload(Audio& audio) {
    if (playerDead_ || player_.reloading) return;
    const WeaponDef& w = WEAPONS[weapon_];
    if (loaded_ >= w.mag) return;
    if (ammo_[w.ammo] <= 0) {
        if (loaded_ <= 0) switchToKnife(audio);
        else {
            toast("没有备用弹药", C_DANGER);
            audio.play(SFX_EMPTY, .5f);
        }
        return;
    }
    knifeOut_ = false;
    knifeManual_ = false;
    player_.reloading = true;
    player_.reloadT = w.reload;
    audio.play(SFX_RELOAD, .6f);
}

void Raid::finishReload() {
    const WeaponDef& w = WEAPONS[weapon_];
    int need = w.mag - loaded_;
    int take = std::min(need, ammo_[w.ammo]);
    loaded_ += take;
    ammo_[w.ammo] -= take;
    if (take <= 0) toast("换弹失败：备弹不足", C_DANGER);
}

void Raid::useMed(int kind, Audio& audio) {
    if (playerDead_ || player_.healT > 0) return;
    if (kind == 1) {
        if (bandages_ <= 0) { toast("没有绷带", C_DANGER); return; }
        if (player_.hp >= player_.maxHp - .1f) { toast("生命值已满，不需要吃药", C_MUTED); return; }
        --bandages_;
        player_.healT = .95f;
        player_.healKind = 1;
        audio.play(SFX_HEAL, .35f, 1.2f);
        toast("正在包扎…", C_ACCENT);
    } else {
        if (medkits_ <= 0) { toast("没有止痛针", C_DANGER); return; }
        if (player_.hp >= player_.maxHp - .1f) { toast("生命值已满，不需要吃药", C_MUTED); return; }
        --medkits_;
        player_.healT = 1.45f;
        player_.healKind = 2;
        audio.play(SFX_HEAL, .35f, 1.2f);
        toast("正在注射蛆蛋白…", C_ACCENT);
    }
}

void Raid::fireWeapon(Input& in, Audio& audio) {
    (void)in;
    if (loaded_ <= 0 || player_.healT > 0) return;
    const WeaponDef& w = WEAPONS[weapon_];
    --loaded_;
    player_.fireCd = 60.f / std::max(1.f, w.rpm);

    float moveSpread = clampF(length(player_.vel) / 190.f, 0.f, 1.25f) * 2.6f;
    float baseSpread = w.spread + moveSpread;
    Vec2 muzzle = player_.pos + fromAngle(player_.angle, 19.f) + fromAngle(player_.angle + PI * .5f, 5.f);
    bool anyHit = false, anyCrit = false;

    for (int p = 0; p < std::max(1, w.pellets); ++p) {
        float spread = baseSpread;
        if (w.pellets > 1) spread += 1.7f;
        float a = player_.angle + rng_.rangeF(-spread, spread) * PI / 180.f;
        Vec2 dir = fromAngle(a, 1.f);
        Vec2 end = muzzle + dir * w.range;
        float wallT = map_.wallRay(muzzle, end);
        if (wallT < 1.f) end = lerp(muzzle, end, wallT);

        float bestT = 2.f;
        int best = -1;
        for (int i = 0; i < (int)enemies_.size(); ++i) {
            Enemy& e = enemies_[(size_t)i];
            if (!e.alive) continue;
            float t = rayCircle(muzzle, end, e.pos, e.radius + 2.f);
            if (t >= 0.f && t < bestT) { bestT = t; best = i; }
        }

        if (best >= 0) {
            Enemy& e = enemies_[(size_t)best];
            Vec2 hitPos = lerp(muzzle, end, bestT);
            bool crit = rng_.chance(w.pellets > 1 ? .035f : .08f);
            float falloff = 1.f - .42f * clamp01(bestT);
            float dmg = w.damage * falloff * (crit ? 1.65f : 1.f) * rng_.rangeF(.9f, 1.1f);
            damageEnemy(e, dmg, crit, hitPos, audio);
            anyHit = true;
            anyCrit = anyCrit || crit;
            end = hitPos;
        } else if (wallT < 1.f) {
            addParticles(end, 0, 3, Col(190, 180, 130, 210), rng_.rangeF(30, 100), .22f, 1.5f);
            if (rng_.chance(.3f)) {
                Decal d;
                d.pos = end;
                d.rx = 2.2f; d.ry = 2.2f;
                d.color = Col(8, 7, 5, 220);
                d.kind = 3;
                decals_.push_back(d);
            }
        }
        addTracer(muzzle, end, Col(245, 224, 142, 190), 1.15f);
    }

    player_.muzzle = .065f;
    addParticles(muzzle, 0, 4, Col(255, 220, 120, 230), 120.f, .12f, 2.3f);
    addParticles(muzzle, 2, 2, Col(120, 120, 100, 90), 40.f, .5f, 6.f);
    Particle shell;
    shell.pos = player_.pos + fromAngle(player_.angle + PI * .5f, 10.f);
    shell.vel = fromAngle(player_.angle + PI * .5f + rng_.rangeF(-.4f, .4f), rng_.rangeF(50, 120));
    shell.life = shell.maxLife = .7f;
    shell.size = 2.2f; shell.color = Col(190, 160, 80, 240); shell.kind = 3; shell.drag = 3.f;
    particles_.push_back(shell);

    shake_ += weapon_ == WPN_RIFLE ? 3.6f : (weapon_ == WPN_SHOTGUN ? 3.f : 1.5f);
    ++shots_;
    if (anyHit) {
        ++hitTriggers_;
        hitMarker_ = .16f;
        critMarker_ = anyCrit;
        audio.play(SFX_HIT, .42f, anyCrit ? 1.35f : 1.f);
    }
    audio.play(weaponSfx(weapon_), .9f, rng_.rangeF(.95f, 1.05f));
    alertEnemies(player_.pos, weapon_ == WPN_RIFLE ? 820.f : 620.f);

    if (loaded_ == 0) {
        if (ammo_[w.ammo] > 0) startReload(audio);
        else switchToKnife(audio);
    }
}

void Raid::switchToKnife(Audio& audio, bool manual) {
    if (knifeOut_ || playerDead_) return;
    knifeOut_ = true;
    knifeManual_ = manual;
    player_.reloading = false;
    player_.reloadT = 0;
    toast(manual ? "收起枪械 — 蛆牙短刀" : "弹药耗尽 — 自动切刀", manual ? C_ACCENT : C_GOLD);
    audio.play(manual ? SFX_UI_CLICK : SFX_EMPTY, .45f, .7f);
}

void Raid::toggleKnife(Audio& audio) {
    if (playerDead_) return;
    if (!knifeOut_) {
        switchToKnife(audio, true);
        return;
    }
    const WeaponDef& w = WEAPONS[weapon_];
    if (w.id == WPN_NONE) {
        toast("跑刀模式没有枪可切", C_MUTED);
        return;
    }
    if (loaded_ <= 0 && ammo_[w.ammo] <= 0) {
        toast("没有弹药，无法切回枪械", C_DANGER);
        audio.play(SFX_EMPTY, .35f, .6f);
        return;
    }
    knifeOut_ = false;
    knifeManual_ = false;
    player_.reloading = false;
    player_.reloadT = 0;
    toast("切回枪械", C_ACCENT);
    audio.play(SFX_UI_CLICK, .5f, 1.1f);
    if (loaded_ <= 0) startReload(audio);
}

void Raid::meleeAttack(Audio& audio) {
    if (playerDead_ || player_.healT > 0 || player_.fireCd > 0) return;
    player_.fireCd = .42f;
    Vec2 blade = player_.pos + fromAngle(player_.angle, 27.f);
    int best = -1;
    float bestD = 1e9f;
    for (int i = 0; i < (int)enemies_.size(); ++i) {
        Enemy& e = enemies_[(size_t)i];
        if (!e.alive) continue;
        Vec2 rel = e.pos - player_.pos;
        float d = length(rel);
        if (d > 64.f + e.radius) continue;
        if (std::fabs(angDiff(angleOf(rel), player_.angle)) > 1.05f) continue;
        if (!map_.lineOfSight(player_.pos, e.pos)) continue;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) {
        Enemy& e = enemies_[(size_t)best];
        bool crit = rng_.chance(.12f);
        damageEnemy(e, 34.f * (crit ? 1.5f : 1.f), crit, e.pos, audio);
        hitMarker_ = .16f;
        critMarker_ = crit;
        shake_ = std::max(shake_, 1.5f);
    } else {
        audio.play(SFX_EMPTY, .32f, .55f);
    }
    addParticles(blade, 0, 2, Col(200, 220, 180, 170), 35.f, .18f, 1.3f);
}

void Raid::damageEnemy(Enemy& e, float dmg, bool crit, Vec2 from, Audio& audio) {
    if (!e.alive) return;
    float reduction = clampF(e.armor * .008f, 0.f, .45f);
    int finalDmg = std::max(1, (int)std::lround(dmg * (1.f - reduction)));
    e.hp -= finalDmg;
    e.hitFlash = .12f;
    e.alert = true;
    e.lastKnown = player_.pos;
    e.lostTimer = 7.f;
    e.path.clear();
    e.pathIdx = 0;
    e.pathCd = 0;
    addBlood(from, 5.f);
    addParticles(from, 1, 4, Col(128, 38, 30, 220), rng_.rangeF(25, 120), .55f, 2.6f);
    floatText(e.pos + Vec2(0, -e.radius - 12), std::to_string(finalDmg), crit ? C_GOLD : C_WHITE, crit ? 22 : 17);
    audio.play(SFX_HIT, .32f, crit ? 1.3f : 1.f);
    if (e.hp <= 0) {
        killEnemy(e, audio);
        return;
    }
    if (e.boss && !e.enraged && e.hp < e.maxHp * .4f) {
        e.enraged = true;
        e.speed *= 1.22f;
        e.interval *= .72f;
        e.damage *= 1.28f;
        e.accuracy = std::min(.94f, e.accuracy + .1f);
        toast("蛆王暴怒 — 它不再留手", C_DANGER);
        audio.play(SFX_BOSS, .9f);
        shake_ = std::max(shake_, 16.f);
    }
}

void Raid::killEnemy(Enemy& e, Audio& audio) {
    if (!e.alive) return;
    e.alive = false;
    e.deathTime = 0;
    ++kills_;
    addBlood(e.pos + Vec2(0, 6), e.boss ? 34.f : 16.f);
    Decal d;
    d.pos = e.pos + Vec2(0, 8);
    d.rx = e.boss ? 34.f : 18.f;
    d.ry = e.boss ? 25.f : 12.f;
    d.color = e.boss ? Col(92, 20, 18, 190) : Col(88, 24, 20, 150);
    d.kind = 1;
    decals_.push_back(d);
    dropCorpse(e, audio);
    audio.play(SFX_DEATH, e.boss ? .9f : .55f, e.boss ? .6f : rng_.rangeF(.9f, 1.1f));
    if (e.boss) {
        bossKilled_ = true;
        toast("蛆王已死 — 巢穴在颤抖", C_GOLD);
        audio.play(SFX_BOSS, .7f, .7f);
        shake_ = std::max(shake_, 20.f);
        alertEnemies(e.pos, 900.f);
    }
}

void Raid::dropCorpse(Enemy& e, Audio& audio) {
    (void)audio;
    LootBox b;
    b.pos = e.pos + Vec2(rng_.rangeF(-9, 9), rng_.rangeF(-5, 12));
    b.kind = e.boss ? 4 : 3;
    b.radius = e.boss ? 30.f : 20.f;
    b.pulse = rng_.rangeF(0, TAU);
    b.discovered = true;
    b.items = rollLoot(b.kind, rng_);
    if (e.boss) stackAdd(b.items, IT_GLAND, 1);
    boxes_.push_back(std::move(b));
}

void Raid::damagePlayer(float dmg, Vec2 from, Audio& audio) {
    if (playerDead_ || dmg <= 0) return;
    float raw = dmg;
    if (player_.armorHp > 0) {
        float reduction = ARMORS[armor_].reduction;
        dmg *= (1.f - reduction);
        player_.armorHp = std::max(0.f, player_.armorHp - raw * .78f);
        if (player_.armorHp <= 0) {
            toast("护甲碎裂！", C_DANGER);
            audio.play(SFX_HIT, .6f, .7f);
        }
    }
    player_.hp -= dmg;
    player_.sinceDamage = 0;
    damageFlash_ = 1.f;
    shake_ = std::max(shake_, 6.f);
    lastHitDir_ = normalize(player_.pos - from);
    lastHitTimer_ = 1.1f;
    addBlood(player_.pos, 8.f);
    addParticles(player_.pos, 1, 6, Col(145, 34, 28, 220), rng_.rangeF(30, 140), .7f, 3.f);
    floatText(player_.pos + Vec2(0, -34), "-" + std::to_string((int)std::lround(dmg)), C_DANGER, 19);
    audio.play(SFX_HURT, .75f, rng_.rangeF(.92f, 1.04f));
    if (player_.hp <= 0) {
        player_.hp = 0;
        playerDead_ = true;
        deathTimer_ = 1.8f;
        deathReason_ = "你被撕成了蛆饲料。";
        paused_ = false;
        mapOpen_ = false;
        extractProgress_ = 0;
        searchProgress_ = 0;
        addBlood(player_.pos + Vec2(0, 10), 30.f);
        audio.play(SFX_DEATH, .9f);
    }
}

void Raid::checkPlayerDeath() {
    if (playerDead_ && deathTimer_ <= 0.f && !done_) finish(false, deathReason_);
}

bool Raid::canSeePlayer(const Enemy& e) const {
    if (playerDead_) return false;
    Vec2 to = player_.pos - e.pos;
    float d = length(to);
    float vision = e.vision * (player_.lightOn ? 1.16f : .74f);
    if (d > vision) return false;
    if (d > 165.f) {
        float diff = std::fabs(angDiff(angleOf(to), e.angle));
        if (diff > 1.18f) return false;
    }
    return map_.lineOfSight(e.pos, player_.pos);
}

void Raid::updateEnemies(float dt, Audio& audio) {
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& e = enemies_[i];
        if (!e.alive) {
            e.deathTime += dt;
            continue;
        }
        e.anim += dt;
        e.hitFlash = std::max(0.f, e.hitFlash - dt);
        e.muzzle = std::max(0.f, e.muzzle - dt);
        e.fireCd = std::max(0.f, e.fireCd - dt);
        e.meleeCd = std::max(0.f, e.meleeCd - dt);
        e.thinkCd -= dt;
        e.pathCd -= dt;

        bool sees = canSeePlayer(e);
        float d = dist(e.pos, player_.pos);
        bool wasAlert = e.alert;

        if (sees) {
            if (!e.alert) {
                e.alert = true;
                e.lostTimer = e.boss ? 9.f : 6.f;
                e.idleTimer = 0;
                e.path.clear();
                if (alertSoundCd_ <= 0.f) {
                    audio.play(SFX_ALERT, e.boss ? .75f : .44f, e.boss ? .68f : rng_.rangeF(.95f, 1.08f));
                    alertSoundCd_ = 1.5f;
                }
            }
            e.lastKnown = player_.pos;
            e.lostTimer = e.boss ? 9.f : 6.f;
        } else if (e.alert) {
            e.lostTimer -= dt;
            if (e.lostTimer <= 0.f) {
                e.alert = false;
                e.idleTimer = 0;
                e.path.clear();
            }
        }
        (void)wasAlert;

        auto moveToward = [&](Vec2 target, float speedMul, bool allowDirect) -> bool {
            Vec2 to = target - e.pos;
            float dd = length(to);
            if (dd < 4.f) return true;
            bool direct = allowDirect && dd < 540.f && map_.lineOfSight(e.pos, target);
            if (direct) {
                e.path.clear();
            } else {
                bool needPath = e.path.empty() || e.pathCd <= 0.f || dist(e.pathGoal, target) > 64.f;
                if (needPath) {
                    std::vector<Vec2> np;
                    if (map_.path(e.pos, target, np)) {
                        e.path = std::move(np);
                        e.pathIdx = 0;
                        e.pathGoal = target;
                    } else {
                        e.path.clear();
                    }
                    e.pathCd = .7f + rng_.rangeF(0.f, .25f);
                }
            }
            Vec2 wp = target;
            if (!direct && !e.path.empty()) {
                while (e.pathIdx < e.path.size() && dist(e.pos, e.path[e.pathIdx]) < 15.f) ++e.pathIdx;
                if (e.pathIdx >= e.path.size()) {
                    e.path.clear();
                    e.pathIdx = 0;
                } else {
                    wp = e.path[e.pathIdx];
                }
            }
            Vec2 before = e.pos;
            Vec2 dir = normalize(wp - e.pos);
            moveCircle(e.pos, dir * e.speed * speedMul * dt, e.radius);
            Vec2 moved = e.pos - before;
            e.vel = moved / std::max(dt, .0001f);
            float want = e.speed * speedMul * dt;
            if (length(moved) < want * .12f) {
                e.stuckTimer += dt;
                if (e.stuckTimer > 1.1f) {
                    e.path.clear();
                    e.pathCd = 0;
                    e.stuckTimer = 0;
                }
            } else {
                e.stuckTimer = 0;
            }
            if (sees) e.angle = approachAngle(e.angle, angleOf(player_.pos - e.pos), dt * 7.f);
            else if (length(moved) > .2f) e.angle = approachAngle(e.angle, angleOf(moved), dt * 5.f);
            return dd < 8.f;
        };

        if (e.alert) {
            bool inRange = sees && d <= e.range;
            if (inRange) {
                e.path.clear();
                e.angle = approachAngle(e.angle, angleOf(player_.pos - e.pos), dt * 8.f);
                if (e.fireCd <= 0.f) enemyShoot(e, audio);
            } else {
                Vec2 target = sees ? player_.pos : e.lastKnown;
                moveToward(target, e.enraged ? 1.28f : 1.f, true);
                if (!sees && dist(e.pos, e.lastKnown) < 42.f) {
                    e.angle += dt * .6f;
                }
            }
        } else {
            // Patrol: stand and scan, then pick a point in the current room.
            if (e.path.empty()) {
                if (e.idleTimer > 0.f) {
                    e.idleTimer -= dt;
                    e.angle += dt * .32f * (hash01((int)e.pos.x, (int)e.pos.y) > .5f ? 1.f : -1.f);
                } else {
                    Vec2 p = randomPatrolPoint(e);
                    e.patrol = p;
                    std::vector<Vec2> np;
                    if (map_.path(e.pos, p, np)) {
                        e.path = std::move(np);
                        e.pathIdx = 0;
                        e.pathGoal = p;
                    }
                    e.idleTimer = rng_.rangeF(4.f, 8.f);
                    e.pathCd = .8f;
                }
            } else {
                Vec2 before = e.pos;
                while (e.pathIdx < e.path.size() && dist(e.pos, e.path[e.pathIdx]) < 15.f) ++e.pathIdx;
                if (e.pathIdx >= e.path.size()) {
                    e.path.clear();
                    e.pathIdx = 0;
                    e.idleTimer = rng_.rangeF(1.5f, 4.f);
                } else {
                    Vec2 wp = e.path[e.pathIdx];
                    Vec2 dir = normalize(wp - e.pos);
                    moveCircle(e.pos, dir * e.speed * .55f * dt, e.radius);
                    e.angle = approachAngle(e.angle, angleOf(wp - e.pos), dt * 4.f);
                }
                e.vel = (e.pos - before) / std::max(dt, .0001f);
            }
        }

        if (d < e.radius + player_.radius + 5.f && e.meleeCd <= 0.f) {
            damagePlayer(e.boss ? 30.f : (e.type == EN_BRUTE ? 18.f : 11.f), e.pos, audio);
            e.meleeCd = e.boss ? .95f : 1.25f;
            addParticles(player_.pos, 1, 6, Col(120, 28, 24, 220), 80.f, .5f, 2.5f);
            player_.pos += normalize(player_.pos - e.pos) * 13.f;
        }
        if (playerDead_) break;
    }

    // Soft separation so a pack does not collapse into one polygon.
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& a = enemies_[i];
        if (!a.alive) continue;
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Enemy& b = enemies_[j];
            if (!b.alive) continue;
            Vec2 d = b.pos - a.pos;
            float dd = length(d);
            float want = a.radius + b.radius + 2.f;
            if (dd > .001f && dd < want) {
                Vec2 push = d / dd * (want - dd) * .5f;
                moveCircle(a.pos, push * -1.f, a.radius);
                moveCircle(b.pos, push, b.radius);
            }
        }
        Vec2 d = player_.pos - a.pos;
        float dd = length(d);
        float want = a.radius + player_.radius;
        if (!playerDead_ && dd > .001f && dd < want) {
            Vec2 push = d / dd * (want - dd) * .5f;
            player_.pos += push;
            moveCircle(a.pos, push * -1.f, a.radius);
        }
    }
}

void Raid::enemyShoot(Enemy& e, Audio& audio) {
    if (!e.alive || playerDead_) return;
    Vec2 to = player_.pos - e.pos;
    float d = length(to);
    if (d < .001f) return;
    float spread = (1.f - e.accuracy) * .18f + .015f + (e.enraged ? .015f : 0.f);
    float a = angleOf(to) + rng_.rangeF(-spread, spread);
    Vec2 dir = fromAngle(a, 1.f);
    Vec2 muzzle = e.pos + dir * (e.radius + 4.f);
    Vec2 end = muzzle + dir * e.range;
    float wallT = map_.wallRay(muzzle, end);
    if (wallT < 1.f) end = lerp(muzzle, end, wallT);
    float hitT = rayCircle(muzzle, end, player_.pos, player_.radius + 2.f);
    bool hit = hitT >= 0.f && hitT < 1.f;
    if (hit) {
        end = lerp(muzzle, end, hitT);
        damagePlayer(e.damage * rng_.rangeF(.9f, 1.1f), e.pos, audio);
    }
    addTracer(muzzle, end, e.boss ? Col(255, 198, 92, 210) : Col(244, 128, 96, 180), e.boss ? 2.f : 1.3f);
    addParticles(muzzle, 0, 2, e.boss ? C_GOLD : Col(250, 160, 90, 220), 70.f, .12f, 1.8f);
    e.muzzle = .06f;
    e.fireCd = e.interval * rng_.rangeF(e.enraged ? .68f : .86f, e.enraged ? .9f : 1.16f);
    e.angle = a;
    audio.play(enemySfx(e.type), .55f, rng_.rangeF(.94f, 1.06f));
    alertEnemies(e.pos, 620.f);
}

void Raid::alertEnemies(Vec2 p, float radius) {
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        if (dist(e.pos, p) < radius) {
            e.alert = true;
            e.lastKnown = player_.pos;
            e.lostTimer = e.boss ? 9.f : 6.f;
            e.path.clear();
            e.pathCd = 0;
        }
    }
}

void Raid::updateEffects(float dt) {
    for (auto& p : particles_) {
        p.life -= dt;
        p.pos += p.vel * dt;
        p.vel = p.vel * std::max(0.f, 1.f - p.drag * dt);
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle& p) { return p.life <= 0; }), particles_.end());
    for (auto& t : tracers_) t.life -= dt;
    tracers_.erase(std::remove_if(tracers_.begin(), tracers_.end(),
        [](const Tracer& t) { return t.life <= 0; }), tracers_.end());
    for (auto& f : floats_) {
        f.life -= dt;
        f.pos.y -= dt * 24.f;
    }
    floats_.erase(std::remove_if(floats_.begin(), floats_.end(),
        [](const FloatText& f) { return f.life <= 0; }), floats_.end());
    for (auto& t : toasts_) t.life -= dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
        [](const Toast& t) { return t.life <= 0; }), toasts_.end());
    for (auto& b : boxes_) {
        b.pulse += dt;
        if (!b.discovered && pointVisible(b.pos)) b.discovered = true;
    }
}

void Raid::interact(float dt, Input& in, Audio& audio) {
    int onExtract = -1;
    for (int i = 0; i < (int)map_.extracts.size(); ++i) {
        if (map_.extracts[(size_t)i].zone.contains(player_.pos)) { onExtract = i; break; }
    }
    if (onExtract >= 0) {
        extractIndex_ = onExtract;
        searchIndex_ = -1;
        searchProgress_ = 0;
        if (in.key(SDL_SCANCODE_E)) {
            extractProgress_ += dt / 3.f;
            if (extractProgress_ >= 1.f) {
                extractProgress_ = 1.f;
                audio.play(SFX_EXTRACT, .9f);
                finish(true, "撤离成功");
            }
        } else {
            extractProgress_ = std::max(0.f, extractProgress_ - dt * 1.4f);
        }
        return;
    }
    extractIndex_ = -1;
    extractProgress_ = std::max(0.f, extractProgress_ - dt * 2.f);

    int best = -1;
    float bestD = 96.f;
    for (int i = 0; i < (int)boxes_.size(); ++i) {
        LootBox& b = boxes_[i];
        if (b.items.empty()) continue;
        float d = dist(b.pos, player_.pos);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best < 0) {
        searchIndex_ = -1;
        searchProgress_ = 0;
        return;
    }
    if (searchIndex_ != best) {
        searchIndex_ = best;
        searchProgress_ = 0;
    }
    LootBox& b = boxes_[(size_t)best];
    if (!b.discovered && pointVisible(b.pos)) b.discovered = true;
    bool holding = in.key(SDL_SCANCODE_E) && length(player_.vel) < 55.f;
    if (holding) {
        searchProgress_ += dt / .95f;
        if (searchProgress_ >= 1.f) {
            searchProgress_ = 0;
            lootBox((size_t)best, audio);
        }
    } else {
        searchProgress_ = std::max(0.f, searchProgress_ - dt * 2.6f);
    }
}

void Raid::lootBox(size_t idx, Audio& audio) {
    if (idx >= boxes_.size()) return;
    LootBox& b = boxes_[idx];
    if (b.items.empty()) return;
    bool first = !b.searched;
    b.searched = true;
    if (first) ++searched_;
    float cap = BACKPACKS[backpack_].capacity;
    std::string got;
    auto appendGot = [&](const std::string& s) {
        if (!got.empty()) got += "、";
        got += s;
    };

    for (size_t i = 0; i < b.items.size();) {
        ItemStack& st = b.items[i];
        int aidx = ammoItemType(st.id);
        if (aidx >= 0) {
            ammo_[aidx] += st.count;
            appendGot(ITEMS[st.id].name + std::string(" ×") + std::to_string(st.count));
            b.items.erase(b.items.begin() + (long)i);
            continue;
        }
        if (st.id == IT_BANDAGE) {
            bandages_ += st.count;
            appendGot("绷带 ×" + std::to_string(st.count));
            b.items.erase(b.items.begin() + (long)i);
            continue;
        }
        if (st.id == IT_MEDKIT) {
            medkits_ += st.count;
            appendGot("急救包 ×" + std::to_string(st.count));
            b.items.erase(b.items.begin() + (long)i);
            continue;
        }
        float w = std::max(.001f, ITEMS[st.id].weight);
        int room = (int)std::floor(std::max(0.f, cap - lootWeight_) / w + .0001f);
        int take = std::min(st.count, std::max(0, room));
        if (take <= 0) { ++i; continue; }
        stackAdd(loot_, st.id, take);
        lootWeight_ += w * take;
        lootValue_ += ITEMS[st.id].value * take;
        appendGot(ITEMS[st.id].name + std::string(" ×") + std::to_string(take));
        st.count -= take;
        if (st.count <= 0) b.items.erase(b.items.begin() + (long)i);
        else ++i;
    }

    if (knifeOut_ && !knifeManual_ && ammo_[WEAPONS[weapon_].ammo] > 0) {
        knifeOut_ = false;
        toast("找到弹药 — 切回枪械", C_ACCENT);
        startReload(audio);
    }
    audio.play(first ? SFX_PICKUP : SFX_UI_CLICK, .7f);
    if (!got.empty()) toast("搜到 " + got, b.kind == 4 ? C_GOLD : C_TEXT);
    if (!b.items.empty()) {
        std::string left = itemSummary(b.items);
        toast("背包已满，留下 " + left, C_DANGER);
    } else if (got.empty()) {
        toast(std::string(boxName(b.kind)) + " 是空的", C_MUTED);
    }
}

void Raid::moveCircle(Vec2& p, Vec2 delta, float radius) {
    Vec2 nx(p.x + delta.x, p.y);
    if (!circleSolid(nx, radius)) p.x = nx.x;
    Vec2 ny(p.x, p.y + delta.y);
    if (!circleSolid(ny, radius)) p.y = ny.y;
}

bool Raid::circleSolid(Vec2 p, float radius) const {
    int ts = map_.tileSize;
    int x0 = (int)std::floor((p.x - radius) / ts);
    int x1 = (int)std::floor((p.x + radius) / ts);
    int y0 = (int)std::floor((p.y - radius) / ts);
    int y1 = (int)std::floor((p.y + radius) / ts);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!map_.solid(x, y)) continue;
            float cx = std::clamp(p.x, x * (float)ts, (x + 1) * (float)ts);
            float cy = std::clamp(p.y, y * (float)ts, (y + 1) * (float)ts);
            if (dist2(p, Vec2(cx, cy)) < radius * radius - .01f) return true;
        }
    }
    return false;
}

Vec2 Raid::clampCamera(Vec2 c) const {
    float mw = map_.w * (float)map_.tileSize;
    float mh = map_.h * (float)map_.tileSize;
    float x = std::clamp(c.x, 0.f, std::max(0.f, mw - VIEW_W));
    float y = std::clamp(c.y, 0.f, std::max(0.f, mh - VIEW_H));
    return {x, y};
}

const Room* Raid::roomAt(Vec2 p) const {
    const Room* best = nullptr;
    float bd = 1e30f;
    for (const auto& r : map_.rooms) {
        float d = dist2(r.center, p);
        if (d < bd) { bd = d; best = &r; }
    }
    return best;
}

Vec2 Raid::randomPatrolPoint(Enemy& e) {
    const Room* room = roomAt(e.pos);
    if (!room) return map_.nearestFloor(e.pos + fromAngle(rng_.rangeF(0, TAU), 180.f));
    for (int i = 0; i < 20; ++i) {
        Vec2 p = map_.randomFloorInRoom(rng_, *room, .7f);
        if (dist(p, e.pos) > 60.f && !map_.solidAt(p)) return p;
    }
    return map_.nearestFloor(e.pos);
}

void Raid::updateVisibility(float dt) {
    visTimer_ -= dt;
    if (visTimer_ > 0.f) return;
    visTimer_ = .105f;
    std::fill(vis_.begin(), vis_.end(), 0);
    float radius = player_.lightOn ? 640.f : 345.f;
    int ts = map_.tileSize;
    int cx = (int)(player_.pos.x / ts);
    int cy = (int)(player_.pos.y / ts);
    int tx = (int)std::ceil(radius / ts) + 1;

    for (int y = cy - tx; y <= cy + tx; ++y) {
        for (int x = cx - tx; x <= cx + tx; ++x) {
            if (!map_.inBounds(x, y)) continue;
            Vec2 p = map_.tileCenter(x, y);
            if (dist(p, player_.pos) > radius) continue;
            if (map_.solid(x, y)) continue;
            if (!map_.lineOfSight(player_.pos, p)) continue;
            vis_[(size_t)y * map_.w + x] = 1;
            map_.at(x, y).seen = 1;
        }
    }
    // Reveal walls adjacent to visible floor, so rooms have edges instead of floating floors.
    for (int y = cy - tx; y <= cy + tx; ++y) {
        for (int x = cx - tx; x <= cx + tx; ++x) {
            if (!map_.inBounds(x, y) || !map_.solid(x, y)) continue;
            bool adj = false;
            for (int oy = -1; oy <= 1 && !adj; ++oy) {
                for (int ox = -1; ox <= 1 && !adj; ++ox) {
                    int nx2 = x + ox, ny2 = y + oy;
                    if (map_.inBounds(nx2, ny2) && vis_[(size_t)ny2 * map_.w + nx2]) adj = true;
                }
            }
            if (adj) {
                vis_[(size_t)y * map_.w + x] = 1;
                map_.at(x, y).seen = 1;
            }
        }
    }
    for (auto& b : boxes_) if (!b.discovered && pointVisible(b.pos)) b.discovered = true;
}

bool Raid::pointVisible(Vec2 p) const {
    float d = dist(p, player_.pos);
    if (d > 780.f) return false;
    if (!map_.lineOfSight(player_.pos, p)) return false;
    if (d < 225.f) return true;
    if (!player_.lightOn) return d < 345.f;
    if (d < 470.f) return true;
    return std::fabs(angDiff(angleOf(p - player_.pos), player_.angle)) < .48f;
}

bool Raid::tileSeen(int x, int y) const {
    if (!map_.inBounds(x, y)) return false;
    return map_.at(x, y).seen != 0;
}

bool Raid::tileVis(int x, int y) const {
    if (!map_.inBounds(x, y)) return false;
    if (vis_.size() != (size_t)map_.w * map_.h) return false;
    return vis_[(size_t)y * map_.w + x] != 0;
}

void Raid::updateCamera(float dt) {
    Vec2 look = fromAngle(player_.angle, 46.f);
    Vec2 target = clampCamera(player_.pos - Vec2(VIEW_W * .5f, VIEW_H * .5f) + look);
    if (dt >= 1.f) camera_ = target;
    else camera_ = lerp(camera_, target, 1.f - std::exp(-dt * 8.f));
}

void Raid::addParticles(Vec2 p, int kind, int n, Col col, float speed, float life, float size) {
    for (int i = 0; i < n; ++i) {
        Particle q;
        q.pos = p + Vec2(rng_.rangeF(-3, 3), rng_.rangeF(-3, 3));
        q.vel = fromAngle(rng_.rangeF(0, TAU), rng_.rangeF(speed * .25f, speed));
        q.life = q.maxLife = life * rng_.rangeF(.6f, 1.15f);
        q.size = size * rng_.rangeF(.65f, 1.25f);
        q.color = col;
        q.kind = kind;
        q.drag = kind == 2 ? 1.2f : 4.f;
        particles_.push_back(q);
    }
    if (particles_.size() > 600) particles_.erase(particles_.begin(), particles_.begin() + 120);
}

void Raid::addBlood(Vec2 p, float amount) {
    int n = (int)std::clamp(amount, 3.f, 26.f);
    addParticles(p, 1, n, Col(118, 26, 24, 220), 60.f + amount * 4.f, .65f, 2.8f);
    Decal d;
    d.pos = p + Vec2(rng_.rangeF(-7, 7), rng_.rangeF(-4, 10));
    d.rx = 4.f + amount * .8f;
    d.ry = 2.5f + amount * .45f;
    d.color = Col(80, 20, 18, (Uint8)std::clamp((int)(70 + amount * 5), 60, 190));
    d.kind = 1;
    decals_.push_back(d);
    if (decals_.size() > 260) decals_.erase(decals_.begin(), decals_.begin() + 40);
}

void Raid::addTracer(Vec2 a, Vec2 b, Col c, float w) {
    Tracer t;
    t.a = a; t.b = b; t.color = c; t.width = w;
    t.life = t.maxLife = .055f;
    tracers_.push_back(t);
    if (tracers_.size() > 100) tracers_.erase(tracers_.begin(), tracers_.begin() + 20);
}

void Raid::toast(const std::string& text, Col color) {
    Toast t;
    t.text = text; t.color = color; t.life = 3.2f;
    toasts_.push_back(t);
    if (toasts_.size() > 5) toasts_.erase(toasts_.begin());
}

void Raid::floatText(Vec2 p, const std::string& text, Col color, int px) {
    FloatText f;
    f.pos = p; f.text = text; f.color = color; f.life = f.maxLife = .85f; f.px = px;
    floats_.push_back(f);
    if (floats_.size() > 40) floats_.erase(floats_.begin());
}

void Raid::finish(bool success, const std::string& reason) {
    if (done_) return;
    done_ = true;
    paused_ = false;
    mapOpen_ = false;
    result_.success = success;
    result_.aborted = !success && reason.find("放弃") != std::string::npos;
    result_.died = !success && !result_.aborted;
    result_.bossKilled = bossKilled_;
    result_.mapIndex = mapIndex_;
    result_.kills = kills_;
    result_.shots = shots_;
    result_.hits = hitTriggers_;
    result_.lootValue = lootValue_;
    result_.containersSearched = searched_;
    for (int i = 0; i < AMMO_COUNT; ++i) result_.ammo[i] = ammo_[i];
    result_.ammo[WEAPONS[weapon_].ammo] += loaded_;
    result_.bandages = bandages_;
    result_.medkits = medkits_;
    result_.time = elapsed_;
    result_.reason = reason;
    result_.loot = loot_;
}

void Raid::resetInteractions() {
    extractIndex_ = -1;
    extractProgress_ = 0;
    searchIndex_ = -1;
    searchProgress_ = 0;
}

void Raid::debugPrepareShot() {
    // A deterministic one-frame tableau used for screenshots and visual QA.
    player_.pos = map_.nearestFloor(map_.spawn + Vec2(60, 30));
    player_.angle = .18f;
    player_.hp = 72.f;
    player_.lightOn = true;
    camera_ = clampCamera(player_.pos - Vec2(VIEW_W * .5f, VIEW_H * .5f) + Vec2(60, 0));
    if (map_.rooms.size() > 2) {
        const Room& r = map_.rooms[map_.rooms.size() - 2];
        for (int i = 0; i < 6; ++i) {
            Vec2 p = map_.nearestFloor(r.center + Vec2(-130.f + i * 48.f, -80.f + (i % 2) * 70.f));
            if (dist(p, player_.pos) > 160.f) addBox(p, i % 3, rng_);
        }
    }
    for (int i = 0; i < 3; ++i) {
        Enemy e;
        e.type = i == 0 ? EN_KING : (i == 1 ? EN_MERC : EN_BRUTE);
        e.pos = map_.nearestFloor(player_.pos + Vec2(150.f + i * 70.f, -40.f + i * 45.f));
        e.angle = PI + .2f;
        const EnemyDef& d = ENEMIES[(int)e.type];
        e.maxHp = e.hp = d.hp; e.armor = d.armor; e.speed = d.speed; e.vision = d.vision;
        e.damage = d.damage; e.interval = d.interval; e.accuracy = d.accuracy; e.range = d.range;
        e.radius = d.radius; e.boss = d.boss; e.alert = true; e.patrol = e.pos; e.lastKnown = player_.pos;
        enemies_.push_back(e);
    }
    updateVisibility(.5f);
    updateCamera(1.f);
    toast("战术画面预览", C_ACCENT);
}


void Raid::debugSetMapOpen(bool open) {
    mapOpen_ = open;
    if (open) {
        paused_ = false;
        map_.revealAll();
    }
}

void Raid::debugSetPaused(bool value) {
    paused_ = value;
    if (value) mapOpen_ = false;
}
