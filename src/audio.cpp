#include "audio.h"
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {
float frand(Rng& r) { return r.f01() * 2.f - 1.f; }

void addTone(std::vector<float>& b, int rate, float f0, float f1, float dur, float amp, float decay, float start = 0.f) {
    int s0 = (int)(start * rate), n = (int)(dur * rate);
    for (int i = 0; i < n; ++i) {
        int idx = s0 + i; if (idx < 0 || idx >= (int)b.size()) continue;
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        float env = std::pow(1.f - t, decay) * std::min(1.f, t * 80.f);
        b[(size_t)idx] += std::sin(6.2831853f * f * i / rate) * amp * env;
    }
}

void addNoise(std::vector<float>& b, int rate, float dur, float amp, float decay, float lowpass, float start = 0.f, Rng* rng = nullptr) {
    Rng local(0x1234ABCDu);
    Rng& r = rng ? *rng : local;
    int s0 = (int)(start * rate), n = (int)(dur * rate);
    float lp = 0;
    for (int i = 0; i < n; ++i) {
        int idx = s0 + i; if (idx < 0 || idx >= (int)b.size()) continue;
        float t = (float)i / n;
        float white = frand(r);
        lp += (white - lp) * lowpass;
        float env = std::pow(1.f - t, decay);
        b[(size_t)idx] += lp * amp * env;
    }
}

void addClick(std::vector<float>& b, int rate, float at, float amp = .3f) {
    addNoise(b, rate, .018f, amp, 3.f, .8f, at);
    addTone(b, rate, 1800, 900, .02f, amp * .3f, 4.f, at);
}

std::vector<i16> finalize(const std::vector<float>& b, float gain = 1.f) {
    std::vector<i16> out(b.size());
    for (size_t i = 0; i < b.size(); ++i) {
        float v = std::clamp(b[i] * gain, -1.f, 1.f);
        out[i] = (i16)std::lround(v * 32767.f);
    }
    return out;
}
} // namespace

void Audio::genSounds() {
    const int R = freq_;
    auto make = [&](float seconds) { return std::vector<float>((size_t)(R * seconds), 0.f); };
    { auto b = make(.16f); addNoise(b, R, .16f, .9f, 20.f, .45f, 0, &rng_); addTone(b, R, 150, 45, .14f, .8f, 12.f); addClick(b, R, .002f, .4f); sounds_[SFX_SHOT_PISTOL] = finalize(b, .55f); }
    { auto b = make(.10f); addNoise(b, R, .10f, .75f, 25.f, .6f, 0, &rng_); addTone(b, R, 220, 80, .07f, .45f, 15.f); sounds_[SFX_SHOT_SMG] = finalize(b, .5f); }
    { auto b = make(.38f); addNoise(b, R, .36f, 1.f, 8.f, .22f, 0, &rng_); addTone(b, R, 95, 28, .34f, 1.f, 6.f); addNoise(b, R, .12f, .6f, 20.f, .7f, .01f, &rng_); sounds_[SFX_SHOT_SHOTGUN] = finalize(b, .65f); }
    { auto b = make(.22f); addNoise(b, R, .2f, .9f, 14.f, .5f, 0, &rng_); addTone(b, R, 210, 55, .18f, .8f, 9.f); addClick(b, R, .003f, .5f); sounds_[SFX_SHOT_RIFLE] = finalize(b, .58f); }
    { auto b = make(.05f); addTone(b, R, 1500, 900, .045f, .5f, 3.f); addNoise(b, R, .035f, .35f, 4.f, .8f); sounds_[SFX_HIT] = finalize(b, .45f); }
    { auto b = make(.26f); addTone(b, R, 110, 50, .24f, .9f, 3.f); addNoise(b, R, .16f, .5f, 7.f, .15f); sounds_[SFX_HURT] = finalize(b, .6f); }
    { auto b = make(.26f); addClick(b, R, .01f, .45f); addClick(b, R, .16f, .5f); addNoise(b, R, .04f, .2f, 5.f, .5f, .20f); sounds_[SFX_RELOAD] = finalize(b, .5f); }
    { auto b = make(.22f); addTone(b, R, 220, 180, .12f, .4f, 4.f); addTone(b, R, 660, 880, .1f, .3f, 3.f, .06f); sounds_[SFX_PICKUP] = finalize(b, .5f); }
    { auto b = make(.42f); addTone(b, R, 880, 880, .1f, .35f, 4.f, 0); addTone(b, R, 1175, 1175, .12f, .35f, 4.f, .08f); addTone(b, R, 1568, 1568, .22f, .3f, 3.f, .16f); sounds_[SFX_COIN] = finalize(b, .45f); }
    { auto b = make(.55f); addTone(b, R, 330, 520, .5f, .3f, 2.f); addNoise(b, R, .28f, .12f, 3.f, .1f); sounds_[SFX_HEAL] = finalize(b, .4f); }
    { auto b = make(1.3f); addTone(b, R, 392, 392, .35f, .35f, 2.f, 0); addTone(b, R, 523, 523, .35f, .35f, 2.f, .28f); addTone(b, R, 659, 659, .5f, .4f, 1.5f, .56f); addTone(b, R, 784, 784, .5f, .25f, 2.f, .58f); sounds_[SFX_EXTRACT] = finalize(b, .5f); }
    { auto b = make(1.4f); addTone(b, R, 220, 55, 1.3f, .6f, 1.5f); addNoise(b, R, .8f, .25f, 2.f, .08f); sounds_[SFX_DEATH] = finalize(b, .55f); }
    { auto b = make(.08f); addClick(b, R, 0, .6f); addTone(b, R, 300, 200, .05f, .3f, 4.f); sounds_[SFX_EMPTY] = finalize(b, .5f); }
    { auto b = make(.55f); addTone(b, R, 170, 90, .52f, .8f, 2.f); addNoise(b, R, .5f, .35f, 3.f, .12f); sounds_[SFX_ALERT] = finalize(b, .55f); }
    { auto b = make(1.8f); addTone(b, R, 72, 38, 1.7f, 1.f, 1.2f); addTone(b, R, 108, 55, 1.5f, .5f, 2.f); addNoise(b, R, 1.4f, .4f, 2.f, .05f); sounds_[SFX_BOSS] = finalize(b, .6f); }
    { auto b = make(1.1f); addTone(b, R, 523, 523, .12f, .3f, 2.f, 0); addTone(b, R, 659, 659, .12f, .3f, 2.f, .12f); addTone(b, R, 784, 784, .12f, .3f, 2.f, .24f); addTone(b, R, 1046, 1046, .5f, .35f, 2.f, .36f); sounds_[SFX_MISSION] = finalize(b, .5f); }
    { auto b = make(.035f); addTone(b, R, 1000, 720, .03f, .15f, 3.f); sounds_[SFX_UI_MOVE] = finalize(b, .4f); }
    { auto b = make(.09f); addTone(b, R, 400, 620, .08f, .3f, 3.f); addClick(b, R, .01f, .2f); sounds_[SFX_UI_CLICK] = finalize(b, .45f); }
    { auto b = make(.5f); addTone(b, R, 587, 587, .16f, .28f, 3.f, 0); addTone(b, R, 880, 880, .3f, .28f, 2.f, .13f); sounds_[SFX_BUY] = finalize(b, .4f); }
}

void Audio::genAmbient(std::vector<i16>& buf, int kind) {
    const int R = freq_;
    buf.assign((size_t)(R * 5.0f), 0);
    std::vector<float> b(buf.size(), 0.f);
    Rng r(kind == 1 ? 0xA11CEu : 0xB0B5u);
    if (kind == 1) {
        for (int i = 0; i < (int)b.size(); ++i) {
            float t = (float)i / R;
            b[(size_t)i] += std::sin(2 * 3.14159265f * 55.f * t) * .035f;
            b[(size_t)i] += std::sin(2 * 3.14159265f * 82.5f * t + 1.3f) * .018f;
            b[(size_t)i] += std::sin(2 * 3.14159265f * 110.f * t + .4f) * .012f;
            b[(size_t)i] += frand(r) * .006f;
        }
    } else {
        float lp = 0;
        for (int i = 0; i < (int)b.size(); ++i) {
            float t = (float)i / R;
            float white = frand(r);
            lp += (white - lp) * .004f;
            b[(size_t)i] += lp * .22f;
            b[(size_t)i] += std::sin(2 * 3.14159265f * 43.f * t) * .035f;
            b[(size_t)i] += std::sin(2 * 3.14159265f * 64.5f * t + .7f) * .012f * (0.5f + 0.5f * std::sin(t * .7f));
        }
        // A distant drip once per loop.
        addTone(b, R, 520, 300, .12f, .05f, 6.f, 2.1f);
    }
    // Fade the loop seam so repeats are not audible.
    int fade = (int)(R * .45f);
    for (int i = 0; i < fade; ++i) {
        float t = (float)i / fade;
        b[(size_t)i] *= t;
        b[b.size() - 1 - (size_t)i] *= t;
    }
    buf = finalize(b, 1.f);
}

bool Audio::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { SDL_Log("audio: disabled (%s)", SDL_GetError()); return false; }
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = &Audio::callback;
    want.userdata = this;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have_, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!dev_) { SDL_Log("audio: open failed (%s)", SDL_GetError()); return false; }
    freq_ = have_.freq;
    channels_ = have_.channels;
    genSounds();
    for (int i = 0; i < 3; ++i) genAmbient(ambient_[i], i);
    SDL_PauseAudioDevice(dev_, 0);
    return true;
}

void Audio::shutdown() {
    if (dev_) { SDL_CloseAudioDevice(dev_); dev_ = 0; }
    for (auto& s : sounds_) s.clear();
    for (auto& a : ambient_) a.clear();
    for (auto& v : voices_) v = Voice{};
}

int Audio::findVoice(bool ambient) {
    if (ambient) return 0; // voice 0 is reserved for the looping ambience
    for (int i = 1; i < (int)voices_.size(); ++i) if (!voices_[(size_t)i].active) return i;
    // Steal the oldest non-ambient voice.
    for (int i = 1; i < (int)voices_.size(); ++i) return i;
    return -1;
}

void Audio::play(Sfx s, float volume, float pitch) {
    if (!dev_ || muted_ || (int)s < 0 || (int)s >= SFX_COUNT) return;
    const auto& smp = sounds_[(size_t)s];
    if (smp.empty()) return;
    SDL_LockAudioDevice(dev_);
    int idx = findVoice(false);
    if (idx >= 0) {
        Voice& v = voices_[(size_t)idx];
        v.sample = &smp; v.pos = 0; v.step = pitch; v.gain = volume * master_;
        v.loop = false; v.ambient = false; v.active = true;
    }
    SDL_UnlockAudioDevice(dev_);
}

void Audio::setAmbient(int kind) {
    if (!dev_ || ambientKind_ == kind) return;
    ambientKind_ = kind;
    SDL_LockAudioDevice(dev_);
    Voice& amb = voices_[0];
    amb.active = false;
    if (kind > 0 && kind < 3) {
        amb.sample = &ambient_[kind];
        amb.pos = 0; amb.step = 1.f; amb.gain = (kind == 1 ? .24f : .20f) * master_;
        amb.loop = true; amb.ambient = true; amb.active = true;
    }
    SDL_UnlockAudioDevice(dev_);
}

void Audio::callback(void* user, Uint8* stream, int len) {
    Audio* a = (Audio*)user;
    int frames = len / (int)sizeof(i16) / a->channels_;
    std::vector<i16> mono((size_t)frames, 0);
    a->mix(mono.data(), frames);
    i16* out = (i16*)stream;
    if (a->channels_ == 1) {
        std::memcpy(out, mono.data(), (size_t)frames * sizeof(i16));
    } else {
        for (int i = 0; i < frames; ++i) for (int c = 0; c < a->channels_; ++c) out[i * a->channels_ + c] = mono[(size_t)i];
    }
}

void Audio::mix(i16* out, int frames) {
    std::vector<int32_t> acc((size_t)frames, 0);
    for (auto& v : voices_) {
        if (!v.active || !v.sample) continue;
        const auto& smp = *v.sample;
        for (int i = 0; i < frames; ++i) {
            size_t p = (size_t)v.pos;
            if (p + 1 >= smp.size()) {
                if (v.loop) { v.pos = 0; p = 0; }
                else { v.active = false; break; }
            }
            float a = smp[p], b = smp[p + 1];
            float sample = a + (b - a) * (float)(v.pos - (double)p);
            acc[(size_t)i] += (int32_t)(sample * v.gain);
            v.pos += v.step;
        }
    }
    for (int i = 0; i < frames; ++i) {
        int32_t v = (int32_t)(acc[(size_t)i] * (muted_ ? 0.f : 1.f));
        out[i] = (i16)std::clamp(v, -32768, 32767);
    }
}
