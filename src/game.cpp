#include "game.h"
#include <fstream>
#include <cstdio>

namespace {
std::string pathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

bool fileExists(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return (bool)f;
}
} // namespace

bool Game::init(const GameOptions& opts) {
    opts_ = opts;
    screenshotMode_ = opts.screenshot;

    if (screenshotMode_) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    }

    // Must be set before video/IME initialization.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "0");
    SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    // This is an action game, not a text field.  Leaving SDL text input active
    // lets desktop IMEs (fcitx/ibus) swallow WASD/E/R and turn them into pinyin.
    Uint32 wflags = screenshotMode_ ? SDL_WINDOW_HIDDEN : (SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    window_ = SDL_CreateWindow("暗蛆突围 · DARK MAGGOT BREAKOUT",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               (int)VIEW_W, (int)VIEW_H, wflags);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_StopTextInput();

    Uint32 rflags = screenshotMode_ ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    renderer_ = SDL_CreateRenderer(window_, -1, rflags);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer_, (int)VIEW_W, (int)VIEW_H);
    SDL_SetWindowMinimumSize(window_, 960, 600);

    char* baseRaw = SDL_GetBasePath();
    std::string base = baseRaw ? baseRaw : "";
    if (baseRaw) SDL_free(baseRaw);

    const std::string candidates[] = {
        pathJoin(base, "assets/font.bin"),
        pathJoin(base, "../assets/font.bin"),
        "assets/font.bin",
        "../assets/font.bin",
        pathJoin(base, "../../assets/font.bin"),
    };
    bool fontOk = false;
    for (const auto& c : candidates) {
        if (fileExists(c) && font_.load(c, renderer_)) {
            assetPath_ = c;
            fontOk = true;
            break;
        }
    }
    if (!fontOk) {
        std::fprintf(stderr, "无法加载 assets/font.bin；请先运行 python3 tools/gen_font.py\n");
        return false;
    }

    ui_.init(renderer_, &font_, &input_, &audio_);
    if (!visual_.init(renderer_)) {
        std::fprintf(stderr, "visual init failed: %s\n", SDL_GetError());
        return false;
    }
    if (!audio_.init()) {
        std::fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
    }

    savePath_ = pathJoin(base, "save.dat");
    if (opts_.noSave) savePath_.clear();

    if (!screenshotMode_ && !opts_.noSave) {
        loadedSave_ = profile_.load(savePath_);
    }
    if (screenshotMode_) {
        // A deterministic, lived-in profile makes the documentation screens look real.
        profile_.coins = 12480;
        profile_.weapon = WPN_SMG;
        profile_.armor = ARM_PLATE;
        profile_.backpack = 2;
        profile_.ammo[0] = 96;
        profile_.ammo[1] = 12;
        profile_.ammo[2] = 20;
        profile_.bandages = 4;
        profile_.medkits = 2;
        profile_.extracted = 7;
        profile_.kills = 34;
        profile_.lootValue = 18600;
        profile_.factoryExtracts = 2;
        profile_.bossKills = 1;
        profile_.raids = 14;
        profile_.deaths = 3;
        profile_.xp = 2140;
        profile_.stash = {
            {IT_SCREW, 4}, {IT_BREAD, 2}, {IT_OIL, 1}, {IT_PHONE, 1},
            {IT_MUSHROOM, 2}, {IT_PROTEIN, 1}, {IT_CHIP, 1}, {IT_EGG, 1},
            {IT_DOGTAG, 2}, {IT_EYE, 1}, {IT_HAIR, 3}, {IT_GLAND, 1},
        };
    }

    audio_.setMaster(profile_.settings.volume);
    audio_.setMuted(profile_.settings.muted);
    audio_.setAmbient(1);
    if (profile_.settings.fullscreen && !screenshotMode_) {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    if (opts_.keydebug) {
        std::fprintf(stderr, "keydebug: driver=%s text_input=%d\n",
                     SDL_GetCurrentVideoDriver(), SDL_IsTextInputActive());
    }
    input_.state = SDL_GetKeyboardState(nullptr);
    input_.mouse = {-100.f, -100.f};
    SDL_ShowCursor(SDL_DISABLE);

    if (screenshotMode_) return screenshotSetup();
    return true;
}

int Game::run() {
    if (!renderer_) return 1;
    if (screenshotMode_) {
        render();
        SDL_RenderPresent(renderer_);
        bool ok = screenshotSave();
        return ok ? 0 : 1;
    }

    u64 last = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();
    while (running_) {
        u64 now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - last) / freq);
        last = now;
        if (dt > .1f) dt = .1f;
        if (dt < 0.f) dt = 0.f;
        hoverTime_ += dt;

        input_.beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running_ = false;
            } else if (e.type == SDL_MOUSEMOTION) {
                float lx = (float)e.motion.x, ly = (float)e.motion.y;
                SDL_RenderWindowToLogical(renderer_, e.motion.x, e.motion.y, &lx, &ly);
                input_.mouse = {lx, ly};
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                float lx = (float)e.button.x, ly = (float)e.button.y;
                SDL_RenderWindowToLogical(renderer_, e.button.x, e.button.y, &lx, &ly);
                input_.mouse = {lx, ly};
                input_.pressPos = input_.mouse;
                input_.down = true;
                input_.pressed = true;
            } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                float lx = (float)e.button.x, ly = (float)e.button.y;
                SDL_RenderWindowToLogical(renderer_, e.button.x, e.button.y, &lx, &ly);
                input_.mouse = {lx, ly};
                input_.down = false;
                input_.released = true;
            } else if (e.type == SDL_MOUSEWHEEL) {
                input_.wheel += e.wheel.y;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Scancode sc = e.key.keysym.scancode;
                if (opts_.keydebug) {
                    std::fprintf(stderr, "KEYDOWN sc=%d sym=0x%x name=%s\n",
                                 (int)sc, (unsigned)e.key.keysym.sym,
                                 SDL_GetKeyName(e.key.keysym.sym));
                }
                input_.held[(size_t)sc] = true;
                input_.keysDown.insert(e.key.keysym.sym);
                if (SDL_Keycode kc = SDL_GetKeyFromScancode(sc)) input_.keysDown.insert(kc);
                // Some remote-desktop / layout combinations report an unknown
                // scancode but a valid keycode.  Recover the scancode from the
                // keycode so physical WASD still maps to the right action.
                if (sc == SDL_SCANCODE_UNKNOWN) {
                    SDL_Scancode derived = SDL_GetScancodeFromKey(e.key.keysym.sym);
                    if (derived != SDL_SCANCODE_UNKNOWN) input_.held[(size_t)derived] = true;
                }
                if (!e.key.repeat) {
                    input_.hit[(size_t)sc] = true;
                    if (sc == SDL_SCANCODE_UNKNOWN) {
                        SDL_Scancode derived = SDL_GetScancodeFromKey(e.key.keysym.sym);
                        if (derived != SDL_SCANCODE_UNKNOWN) input_.hit[(size_t)derived] = true;
                    }
                    input_.keysHit.insert(e.key.keysym.sym);
                }
            } else if (e.type == SDL_KEYUP) {
                SDL_Scancode sc = e.key.keysym.scancode;
                if (opts_.keydebug) {
                    std::fprintf(stderr, "KEYUP   sc=%d sym=0x%x name=%s\n",
                                 (int)sc, (unsigned)e.key.keysym.sym,
                                 SDL_GetKeyName(e.key.keysym.sym));
                }
                input_.held[(size_t)sc] = false;
                if (sc == SDL_SCANCODE_UNKNOWN) {
                    SDL_Scancode derived = SDL_GetScancodeFromKey(e.key.keysym.sym);
                    if (derived != SDL_SCANCODE_UNKNOWN) input_.held[(size_t)derived] = false;
                }
                input_.keysDown.erase(e.key.keysym.sym);
                input_.keysDown.erase(SDL_GetKeyFromScancode(sc));
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                input_.held.fill(false);
                input_.keysDown.clear();
                input_.hit.fill(false);
                input_.keysHit.clear();
                input_.down = false;
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                // Some IMEs re-enable themselves when focus returns.
                SDL_StopTextInput();
            }
        }
        input_.state = SDL_GetKeyboardState(nullptr);

        update(dt);
        render();
        SDL_RenderPresent(renderer_);
    }
    if (!opts_.noSave && !savePath_.empty()) profile_.save(savePath_);
    return 0;
}

void Game::update(float dt) {
    time_ += dt;
    updateNotices(dt);

    if (screen_ == Screen::RAID) {
        raid_.update(dt, input_, audio_);
        if (raid_.finished()) {
            result_ = raid_.result();
            applyResult();
            screen_ = Screen::RESULT;
            audio_.setAmbient(1);
        }
        return;
    }

    if (input_.keyHit(SDL_SCANCODE_ESCAPE)) {
        if (screen_ == Screen::HUB) {
            running_ = false;
        } else {
            setScreen(Screen::HUB);
        }
        return;
    }

    if (screen_ == Screen::DEPLOY && input_.wheel != 0) {
        int next = std::clamp(deployMap_ + (input_.wheel > 0 ? -1 : 1), 0, 2);
        deployMap_ = next;
    }
    if (screen_ == Screen::STASH && input_.wheel != 0) {
        int rows = (int)profile_.stash.size() / 6 + ((int)profile_.stash.size() % 6 ? 1 : 0);
        int maxScroll = std::max(0, rows - 5);
        stashScroll_ = std::clamp(stashScroll_ - input_.wheel, 0, maxScroll);
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 4, 7, 4, 255);
    SDL_RenderClear(renderer_);

    if (screen_ == Screen::RAID) {
        raid_.render(ui_, visual_, time_);
    } else {
        renderMenuScene();
    }

    drawNotices();
    if (!screenshotMode_ && screen_ != Screen::RAID) {
        ui_.cursor(false, input_.mouse);
    }
}

void Game::renderMenuScene() {
    switch (screen_) {
        case Screen::HUB: drawHub(); break;
        case Screen::DEPLOY: drawDeploy(); break;
        case Screen::STASH: drawStash(); break;
        case Screen::MISSIONS: drawMissions(); break;
        case Screen::SHOP: drawShop(); break;
        case Screen::SETTINGS: drawSettings(); break;
        case Screen::RESULT: drawResult(); break;
        default: drawHub(); break;
    }
}

void Game::setScreen(Screen s) {
    screen_ = s;
    if (s == Screen::RAID) audio_.setAmbient(2);
    else audio_.setAmbient(1);
    if (s == Screen::DEPLOY) deployMap_ = std::clamp(deployMap_, 0, 2);
    if (s == Screen::STASH && stashSel_ >= (int)profile_.stash.size()) stashSel_ = -1;
}

void Game::beginRaid() {
    if (!mapUnlocked(deployMap_)) {
        notify("该封锁区尚未解锁", C_DANGER);
        return;
    }
    bool knife = knifeRun_;
    raidKnife_ = knife;
    int effectiveGear = knife ? 0 : profile_.gearValue();
    if (effectiveGear < MAPS[deployMap_].gearReq) {
        notify(std::string(knife ? "跑刀有效战备 0，" : "战备值不足：") + "需要 " +
               formatMoney(MAPS[deployMap_].gearReq) + "，当前 " + formatMoney(effectiveGear), C_DANGER);
        return;
    }
    if (!knife) {
        AmmoId need = WEAPONS[profile_.weapon].ammo;
        if (profile_.weapon == WPN_NONE || profile_.ammo[(int)need] <= 0) {
            notify(std::string("没有") + ammoShort(need) + "弹药，先去黑市补给", C_DANGER);
            return;
        }
    }
    raidSeed_ = opts_.seed ? (opts_.seed + (u64)profile_.raids * 7919u) : (SDL_GetTicks64() ^ 0x9E3779B97F4A7C15ull);
    resultApplied_ = false;
    Profile raidProfile = profile_;
    if (knife) {
        raidProfile.weapon = WPN_NONE;
        raidProfile.armor = ARM_NONE;
        raidProfile.backpack = 0;
        for (auto& a : raidProfile.ammo) a = 0;
        raidProfile.bandages = 0;
        raidProfile.medkits = 0;
    }
    if (!raid_.start(deployMap_, raidSeed_, raidProfile)) {
        notify("封锁区生成失败，请再试一次", C_DANGER);
        return;
    }
    // The click that started the raid must not become the first shot.
    input_.down = false;
    input_.pressed = false;
    input_.released = false;
    audio_.setAmbient(2);
    setScreen(Screen::RAID);
}

void Game::applyResult() {
    if (resultApplied_) return;
    resultApplied_ = true;
    bool knifeRun = raidKnife_;
    resultKnife_ = knifeRun;
    lastXp_ = 0;

    if (result_.success) {
        profile_.raids++;
        profile_.extracted++;
        profile_.kills += result_.kills;
        profile_.lootValue += result_.lootValue;
        if (result_.mapIndex == 2) profile_.factoryExtracts++;
        if (result_.bossKilled) profile_.bossKills++;
        for (auto& st : result_.loot) {
            if (st.id >= 0 && st.id < IT_COUNT && st.count > 0) profile_.addItem((ItemId)st.id, st.count);
        }
        if (!knifeRun) {
            profile_.ammo[0] = result_.ammo[0];
            profile_.ammo[1] = result_.ammo[1];
            profile_.ammo[2] = result_.ammo[2];
            profile_.bandages = result_.bandages;
            profile_.medkits = result_.medkits;
        }
        lastXp_ = 120 + result_.kills * 18 + result_.lootValue / 40 + (result_.bossKilled ? 300 : 0);
        profile_.addXp(lastXp_);
        audio_.play(SFX_MISSION, .75f, .92f);
    } else if (result_.died) {
        profile_.raids++;
        profile_.deaths++;
        profile_.kills += result_.kills;
        lastXp_ = result_.kills * 6;
        profile_.addXp(lastXp_);
        if (!knifeRun) profile_.loseGearOnDeath();
        audio_.play(SFX_DEATH, .9f, .8f);
    } else {
        profile_.raids++;
        if (!knifeRun) {
            profile_.ammo[0] = result_.ammo[0];
            profile_.ammo[1] = result_.ammo[1];
            profile_.ammo[2] = result_.ammo[2];
            profile_.bandages = result_.bandages;
            profile_.medkits = result_.medkits;
        }
    }
    raidKnife_ = false;
    saveGame(true);
}

void Game::saveGame(bool quiet) {
    if (screenshotMode_ || opts_.noSave || savePath_.empty()) return;
    if (!profile_.save(savePath_)) {
        notify("存档写入失败", C_DANGER);
    } else if (!quiet) {
        notify("进度已保存", C_ACCENT);
    }
}

void Game::notify(const std::string& text, Col color) {
    notices_.insert(notices_.begin(), Notice{text, color, 3.4f});
    if (notices_.size() > 4) notices_.pop_back();
}

void Game::updateNotices(float dt) {
    for (auto& n : notices_) n.life -= dt;
    notices_.erase(std::remove_if(notices_.begin(), notices_.end(),
                                  [](const Notice& n) { return n.life <= 0.f; }),
                   notices_.end());
}

void Game::drawNotices() {
    float y = 82.f;
    for (auto& n : notices_) {
        float a = clamp01(n.life / .45f);
        ui_.chamfer({926, y, 322, 34}, 6, Col(10, 14, 9, (Uint8)(230 * a)));
        ui_.chamferOutline({926, y, 322, 34}, 6, withAlpha(n.color, (Uint8)(150 * a)), 1.f);
        ui_.fillF(926, y, 3, 34, withAlpha(n.color, (Uint8)(220 * a)));
        ui_.text(n.text, 940, y + 8, 13, withAlpha(n.color, (Uint8)(240 * a)));
        y += 40.f;
    }
}

void Game::drawBackdrop(float t, Col accent) {
    ui_.gradient({0, 0, VIEW_W, VIEW_H}, Col(12, 18, 12), Col(3, 6, 4), 40);
    for (int x = 0; x <= (int)VIEW_W; x += 64) {
        ui_.fillF((float)x, 0, 1, VIEW_H, Col(70, 92, 60, 7));
    }
    for (int y = 0; y <= (int)VIEW_H; y += 64) {
        ui_.fillF(0, (float)y, VIEW_W, 1, Col(70, 92, 60, 7));
    }
    visual_.drawGlow(renderer_, {960.f, 300.f}, 620.f, accent, .13f);
    visual_.drawGlow(renderer_, {160.f, 700.f}, 500.f, Col(120, 155, 75), .08f);
    // slow drifting scanline and a few toxic motes
    float sy = std::fmod(t * 24.f, VIEW_H + 60.f) - 30.f;
    ui_.fillF(0, sy, VIEW_W, 1, withAlpha(accent, 12));
    for (int i = 0; i < 14; ++i) {
        float x = hash01(i, 7) * VIEW_W;
        float y = std::fmod(hash01(i, 19) * VIEW_H + t * (5.f + hash01(i, 3) * 11.f), VIEW_H);
        ui_.circle({x, y}, 1.f + hash01(i, 5) * 1.6f, withAlpha(accent, 22 + (i % 3) * 6), 6);
    }
    visual_.drawGrain(renderer_, .045f);
    visual_.drawVignette(renderer_, .58f);
}

void Game::drawTopBar(bool back, const std::string& title, const std::string& sub) {
    ui_.fillF(0, 0, VIEW_W, 72, Col(8, 12, 8, 238));
    ui_.fillF(0, 0, VIEW_W, 2, C_ACCENT2);
    ui_.fillF(0, 71, VIEW_W, 1, C_BORDER);
    if (back) {
        if (ui_.button({28, 20, 94, 32}, "< 返回", true, false, 14)) setScreen(Screen::HUB);
        ui_.diamond({154, 36}, 7, C_ACCENT);
        ui_.text("暗蛆突围", 170, 18, 18, C_TEXT);
        ui_.text("DARK MAGGOT", 170, 40, 9, C_MUTED);
    } else {
        ui_.diamond({46, 36}, 9, C_ACCENT);
        ui_.fillF(34, 34, 30, 4, C_ACCENT);
        ui_.text("暗蛆突围", 68, 18, 24, C_TEXT);
        ui_.text("DARK MAGGOT BREAKOUT", 68, 46, 9, C_MUTED);
    }
    if (!title.empty()) {
        ui_.fillF(300, 26, 3, 24, C_ACCENT);
        ui_.text(title, 316, 20, 24, C_TEXT);
        if (!sub.empty()) ui_.text(sub, 318, 47, 11, C_MUTED);
    } else if (sub.empty()) {
        ui_.text("在腐败纪元里，活着就是赚到。", 330, 26, 13, C_MUTED);
    }
    // coins
    ui_.chamfer({1012, 16, 240, 40}, 6, Col(20, 27, 18, 240));
    ui_.chamferOutline({1012, 16, 240, 40}, 6, Col(76, 86, 54, 170), 1.f);
    ui_.text("金币", 1026, 28, 12, C_MUTED);
    ui_.text(formatMoney(profile_.coins), 1240, 24, 20, C_GOLD, ALIGN_RIGHT);
    ui_.text("Lv." + std::to_string(profile_.level()), 900, 30, 15, C_ACCENT);
    ui_.text("XP " + std::to_string(profile_.xpIntoLevel()) + "/" + std::to_string(profile_.xpForNext()),
             948, 33, 10, C_MUTED);
}

void Game::drawStatCard(Rect rc, const std::string& label, const std::string& value, Col accent) {
    ui_.panel(rc, Col(13, 19, 12, 220), C_BORDER, 8);
    ui_.fillF(rc.x, rc.y, 3, rc.h, withAlpha(accent, 190));
    ui_.text(label, rc.x + 18, rc.y + 14, 13, C_MUTED);
    ui_.text(value, rc.x + 18, rc.y + 36, 30, accent);
}

void Game::drawWorm(float x, float y, float s, float t, Col body, Col dark, bool crown) {
    float wob = std::sin(t * 2.1f) * .07f;
    float ang = -.52f + wob;
    Vec2 head{x + std::cos(ang) * 36.f * s, y + std::sin(ang) * 36.f * s};
    for (int i = 9; i >= 0; --i) {
        float u = i / 9.f;
        float px = x + std::cos(ang) * (18.f - 30.f * u) * s + std::sin(t * 2.4f + i * .55f) * 5.f * s;
        float py = y + std::sin(ang) * (18.f - 30.f * u) * s + std::cos(t * 2.1f + i * .4f) * 4.f * s;
        float rad = (15.5f - u * 9.f) * s;
        ui_.circle({px, py}, rad, body, 24);
        ui_.circle({px - rad * .28f, py - rad * .32f}, rad * .35f, mixCol(body, C_WHITE, .30f), 12);
        ui_.circle({px + rad * .25f, py + rad * .34f}, rad * .38f, dark, 12);
        if (i > 0 && i % 3 == 0) {
            ui_.circle({px - rad * .7f, py - rad * .2f}, rad * .12f, dark, 6);
            ui_.circle({px - rad * .58f, py - rad * .68f}, rad * .10f, dark, 6);
        }
    }
    float hr = 18.f * s;
    ui_.circle(head, hr, body, 24);
    ui_.circle(head + Vec2{-4.f * s, -5.f * s}, hr * .32f, Col(242, 250, 218), 12);
    ui_.circle(head + Vec2{5.f * s, -4.f * s}, hr * .32f, Col(242, 250, 218), 12);
    ui_.circle(head + Vec2{-3.f * s, -5.f * s}, hr * .14f, C_INK, 8);
    ui_.circle(head + Vec2{6.f * s, -4.f * s}, hr * .14f, C_INK, 8);
    ui_.line(head + Vec2{hr * .4f, hr * .5f}, head + Vec2{hr * 1.2f, hr * .9f}, dark, 2.f * s);
    ui_.line(head + Vec2{-hr * .4f, hr * .5f}, head + Vec2{-hr * 1.1f, hr * 1.0f}, dark, 2.f * s);
    if (crown) {
        Vec2 base{head.x, head.y - hr * .9f};
        ui_.triangle(base + Vec2{0, -22.f * s}, 10.f * s, -RPI * .5f, C_GOLD);
        ui_.triangle(base + Vec2{-16.f * s, -14.f * s}, 8.f * s, -RPI * .5f, C_GOLD);
        ui_.triangle(base + Vec2{16.f * s, -14.f * s}, 8.f * s, -RPI * .5f, C_GOLD);
    }
}

void Game::drawGearLine(float x, float y, float w) {
    const WeaponDef& wp = WEAPONS[profile_.weapon];
    const ArmorDef& ar = ARMORS[profile_.armor];
    const BackpackDef& bp = BACKPACKS[std::clamp(profile_.backpack, 0, 3)];
    ui_.fillF(x, y, w, 1, C_BORDER);
    ui_.text(wp.name, x, y + 9, 17, C_TEXT);
    ui_.text(ammoShort(wp.ammo), x + w, y + 12, 12, C_MUTED, ALIGN_RIGHT);
    ui_.text(ar.name, x, y + 35, 13, C_MUTED);
    ui_.text(bp.name, x + w, y + 35, 13, C_MUTED, ALIGN_RIGHT);
}

bool Game::mapUnlocked(int i) const {
    if (i < 0 || i >= 3) return false;
    return profile_.extracted >= MAPS[i].unlockExtracts;
}

bool Game::screenshotSetup() {
    input_.mouse = {-100.f, -100.f};
    std::string shot = opts_.shot.empty() ? "hub" : opts_.shot;
    if (shot == "hub") {
        screen_ = Screen::HUB;
    } else if (shot == "deploy" || shot == "deploy_knife") {
        deployMap_ = std::clamp(opts_.map, 0, 2);
        knifeRun_ = shot == "deploy_knife";
        screen_ = Screen::DEPLOY;
    } else if (shot == "stash") {
        stashSel_ = profile_.stash.empty() ? -1 : 0;
        screen_ = Screen::STASH;
    } else if (shot == "missions") {
        screen_ = Screen::MISSIONS;
    } else if (shot == "shop" || shot == "shop_armor" || shot == "shop_bag" || shot == "shop_supply") {
        shopTab_ = shot == "shop_armor" ? 1 : (shot == "shop_bag" ? 2 : (shot == "shop_supply" ? 3 : 0));
        screen_ = Screen::SHOP;
    } else if (shot == "settings") {
        screen_ = Screen::SETTINGS;
    } else if (shot == "raid" || shot == "knife") {
        deployMap_ = std::clamp(opts_.map, 0, 2);
        raidSeed_ = opts_.seed ? opts_.seed : 0xA11CE;
        if (shot == "knife") {
            for (auto& a : profile_.ammo) a = 0;
        }
        if (!raid_.start(deployMap_, raidSeed_, profile_)) return false;
        raid_.debugPrepareShot();
        screen_ = Screen::RAID;
    } else if (shot == "map" || shot == "pause") {
        deployMap_ = std::clamp(opts_.map, 0, 2);
        raidSeed_ = opts_.seed ? opts_.seed : 0xA11CE;
        if (!raid_.start(deployMap_, raidSeed_, profile_)) return false;
        raid_.debugPrepareShot();
        if (shot == "map") raid_.debugSetMapOpen(true);
        else raid_.debugSetPaused(true);
        screen_ = Screen::RAID;
    } else if (shot == "result") {
        result_.success = true;
        result_.mapIndex = 1;
        result_.kills = 6;
        result_.shots = 41;
        result_.hits = 23;
        result_.lootValue = 4860;
        result_.containersSearched = 7;
        result_.time = 318.f;
        result_.bossKilled = false;
        result_.reason = "从生鲜卸货区成功撤离";
        result_.loot = {{IT_CHIP, 1}, {IT_MUSHROOM, 2}, {IT_PHONE, 1}, {IT_DOGTAG, 1}, {IT_OIL, 2}};
        result_.ammo[0] = 63; result_.ammo[1] = 0; result_.ammo[2] = 20;
        result_.bandages = 2; result_.medkits = 1;
        lastXp_ = 441;
        screen_ = Screen::RESULT;
    } else {
        std::fprintf(stderr, "unknown screenshot scene '%s'\n", shot.c_str());
        return false;
    }
    return true;
}

bool Game::screenshotSave() {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, (int)VIEW_W, (int)VIEW_H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        std::fprintf(stderr, "screenshot surface failed: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) != 0) {
        std::fprintf(stderr, "RenderReadPixels failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return false;
    }
    std::string out = opts_.out.empty() ? ("build/shot_" + opts_.shot + ".bmp") : opts_.out;
    bool ok = SDL_SaveBMP(surf, out.c_str()) == 0;
    SDL_FreeSurface(surf);
    if (!ok) {
        std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
        return false;
    }
    std::printf("screenshot: %s\n", out.c_str());
    return true;
}

void Game::shutdown() {
    if (!opts_.noSave && !screenshotMode_ && !savePath_.empty()) profile_.save(savePath_);
    raid_ = Raid{};
    audio_.shutdown();
    visual_.destroy();
    font_.destroy();
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    SDL_Quit();
}
