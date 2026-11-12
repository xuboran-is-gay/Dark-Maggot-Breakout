#pragma once
#include "core.h"
#include "data.h"
#include "font.h"
#include "audio.h"
#include "ui.h"
#include "profile.h"
#include "raid.h"

enum class Screen { HUB, DEPLOY, STASH, MISSIONS, SHOP, SETTINGS, RAID, RESULT };

struct GameOptions {
    bool screenshot = false;
    std::string shot;          // hub / deploy / stash / missions / shop / settings / raid / result
    std::string out;           // BMP output
    int map = 0;
    u64 seed = 0;
    bool noSave = false;
    bool keydebug = false;
};

class Game {
public:
    bool init(const GameOptions& opts);
    int run();
    void shutdown();

private:
    struct Notice {
        std::string text;
        Col color;
        float life = 0.f;
    };

    void update(float dt);
    void render();
    void renderMenuScene();
    void setScreen(Screen s);
    void beginRaid();
    void applyResult();
    bool screenshotSetup();
    bool screenshotSave();
    void saveGame(bool quiet = false);
    void notify(const std::string& text, Col color = C_TEXT);
    void updateNotices(float dt);
    void drawNotices();
    void drawBackdrop(float t, Col accent);
    void drawTopBar(bool back, const std::string& title, const std::string& sub = "");
    void drawWorm(float x, float y, float scale, float t, Col body, Col dark, bool crown = false);
    void drawStatCard(Rect rc, const std::string& label, const std::string& value, Col accent);
    void drawGearLine(float x, float y, float w);

    void drawHub();
    void drawDeploy();
    void drawLoadoutPanel();
    void drawStash();
    void drawMissions();
    void drawShop();
    void drawSettings();
    void drawResult();

    void buyAmmo(AmmoId a);
    void buyMeds(int kind);
    void buyWeapon(WeaponId w);
    void buyArmor(ArmorId a);
    void buyBackpack(int b);
    bool mapUnlocked(int i) const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Font font_;
    Audio audio_;
    UI ui_;
    Visual visual_;
    Input input_;
    Profile profile_;
    Raid raid_;

    GameOptions opts_;
    Screen screen_ = Screen::HUB;
    bool running_ = true;
    bool loadedSave_ = false;
    bool resultApplied_ = false;
    bool confirmReset_ = false;
    bool screenshotMode_ = false;
    bool knifeRun_ = false;
    bool loadoutOpen_ = false;
    bool raidKnife_ = false;
    bool resultKnife_ = false;

    int deployMap_ = 0;
    int stashSel_ = -1;
    int stashScroll_ = 0;
    int shopTab_ = 0;
    int lastXp_ = 0;
    float time_ = 0.f;
    float hoverTime_ = 0.f;
    u64 raidSeed_ = 0;

    std::string savePath_ = "save.dat";
    std::string assetPath_ = "assets/font.bin";
    RaidResult result_;
    std::vector<Notice> notices_;
};
