#pragma once
#include "core.h"
#include <array>

enum Sfx {
    SFX_UI_MOVE = 0, SFX_UI_CLICK, SFX_BUY, SFX_SHOT_PISTOL, SFX_SHOT_SMG,
    SFX_SHOT_SHOTGUN, SFX_SHOT_RIFLE, SFX_HIT, SFX_HURT, SFX_RELOAD, SFX_PICKUP,
    SFX_COIN, SFX_HEAL, SFX_EXTRACT, SFX_DEATH, SFX_EMPTY, SFX_ALERT, SFX_BOSS,
    SFX_MISSION, SFX_COUNT
};

class Audio {
public:
    bool init();
    void shutdown();
    bool available() const { return dev_ != 0; }
    void play(Sfx s, float volume = 1.f, float pitch = 1.f);
    void setAmbient(int kind);          // 0 silent, 1 nest, 2 raid
    void setMaster(float v) { master_ = clamp01(v); }
    float master() const { return master_; }
    void setMuted(bool m) { muted_ = m; }
    bool muted() const { return muted_; }

private:
    struct Voice {
        const std::vector<i16>* sample = nullptr;
        double pos = 0;
        float step = 1.f;
        float gain = 0.f;
        bool loop = false;
        bool ambient = false;
        bool active = false;
    };
    static void callback(void* user, Uint8* stream, int len);
    void mix(i16* out, int frames);
    void genSounds();
    void genAmbient(std::vector<i16>& buf, int kind);
    int findVoice(bool ambient);

    SDL_AudioDeviceID dev_ = 0;
    int freq_ = 44100;
    int channels_ = 1;
    SDL_AudioSpec have_{};
    std::array<std::vector<i16>, SFX_COUNT> sounds_;
    std::vector<i16> ambient_[3];
    std::array<Voice, 48> voices_{};
    float master_ = 0.8f;
    bool muted_ = false;
    int ambientKind_ = 0;
    Rng rng_{0xC0FFEEu};
};
