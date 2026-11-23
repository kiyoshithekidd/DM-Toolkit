#include "sound_fx.h"

#include <cmath>
#include <cstring>

#ifdef __3DS__
#include <3ds.h>
#endif

namespace dmtk {

#ifdef __3DS__

namespace {

constexpr float kSampleRate = 22050.0f;

struct ClipBuffer {
    int16_t*    samples = nullptr;
    uint32_t    count   = 0;
    ndspWaveBuf waveBuf{};
};

ClipBuffer g_clips[SFX_COUNT];

uint32_t sfxRand(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

void allocClip(SoundId id, uint32_t numSamples) {
    ClipBuffer& cb = g_clips[id];
    cb.samples = static_cast<int16_t*>(linearAlloc(numSamples * sizeof(int16_t)));
    cb.count   = cb.samples ? numSamples : 0;
    std::memset(&cb.waveBuf, 0, sizeof(cb.waveBuf));
}

void synthAllClips() {
    const float twoPi = 6.2831853f;
    uint32_t rng = 0x13579BDFu;

    // 1. Soft wooden UI tap (28 ms)
    allocClip(SFX_TAP, static_cast<uint32_t>(kSampleRate * 0.028f));
    for (uint32_t i = 0; i < g_clips[SFX_TAP].count; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float env = std::exp(-95.0f * t);
        float s = std::sin(twoPi * 620.0f * t) * 0.6f + std::sin(twoPi * 1240.0f * t) * 0.25f;
        g_clips[SFX_TAP].samples[i] = static_cast<int16_t>(s * env * 12000.0f);
    }

    // 2. Realistic tumbling dice clacks on a felt-lined wooden tray (580 ms)
    allocClip(SFX_DICE_ROLL, static_cast<uint32_t>(kSampleRate * 0.58f));
    const float hitTimes[10] = {0.005f, 0.042f, 0.088f, 0.125f, 0.178f, 0.235f, 0.298f, 0.365f, 0.440f, 0.515f};
    const float hitAmp[10]   = {0.85f,  0.95f,  0.78f,  0.88f,  0.66f,  0.54f,  0.42f,  0.30f,  0.20f,  0.12f};
    const float hitPitch[10] = {1.00f,  0.88f,  1.08f,  0.93f,  1.05f,  0.86f,  0.97f,  0.90f,  0.84f,  0.80f};
    for (uint32_t i = 0; i < g_clips[SFX_DICE_ROLL].count; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float sum = 0.0f;
        float noise = (static_cast<float>(sfxRand(rng) & 0xFFFFu) / 32768.0f) - 1.0f;

        // Subtle low felt/wood tray thump on initial drop
        if (t < 0.08f) {
            float thumpEnv = std::exp(-55.0f * t);
            sum += std::sin(twoPi * 135.0f * t) * thumpEnv * 0.45f;
        }

        for (int h = 0; h < 10; ++h) {
            if (t >= hitTimes[h]) {
                float dt = t - hitTimes[h];
                if (dt < 0.045f) {
                    float f0 = 520.0f * hitPitch[h];
                    float f1 = 1280.0f * hitPitch[h];
                    float bodyEnv  = std::exp(-160.0f * dt);
                    float clickEnv = std::exp(-520.0f * dt);
                    float clack = (std::sin(twoPi * f0 * dt) * 0.50f +
                                   std::sin(twoPi * f1 * dt) * 0.25f) * bodyEnv +
                                  noise * 0.35f * clickEnv;
                    sum += clack * hitAmp[h];
                }
            }
        }
        float clamped = (sum > 1.0f) ? 1.0f : ((sum < -1.0f) ? -1.0f : sum);
        g_clips[SFX_DICE_ROLL].samples[i] = static_cast<int16_t>(clamped * 15500.0f);
    }

    // 3. Natural 20 celebratory golden chime (420 ms)
    allocClip(SFX_NAT20, static_cast<uint32_t>(kSampleRate * 0.42f));
    for (uint32_t i = 0; i < g_clips[SFX_NAT20].count; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float env = std::exp(-5.5f * t) * (t < 0.01f ? (t / 0.01f) : 1.0f);
        float s = std::sin(twoPi * 523.25f * t) * 0.35f +
                  std::sin(twoPi * 659.25f * t) * 0.30f +
                  std::sin(twoPi * 783.99f * t) * 0.25f +
                  std::sin(twoPi * 1046.5f * t) * 0.20f;
        g_clips[SFX_NAT20].samples[i] = static_cast<int16_t>(s * env * 14000.0f);
    }

    // 4. Natural 1 hollow low thud (240 ms)
    allocClip(SFX_NAT1, static_cast<uint32_t>(kSampleRate * 0.24f));
    for (uint32_t i = 0; i < g_clips[SFX_NAT1].count; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float env = std::exp(-12.0f * t);
        float freq = 120.0f - 55.0f * (t / 0.24f);
        float s = std::sin(twoPi * freq * t);
        g_clips[SFX_NAT1].samples[i] = static_cast<int16_t>(s * env * 15000.0f);
    }

    for (int id = 0; id < SFX_COUNT; ++id) {
        if (g_clips[id].samples && g_clips[id].count > 0) {
            DSP_FlushDataCache(g_clips[id].samples, g_clips[id].count * sizeof(int16_t));
        }
    }
}

} // namespace

bool SoundFx::init() {
    if (initialized_) return true;
    if (R_FAILED(ndspInit())) {
        initialized_ = false;
        return false;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for (int ch = 0; ch < 2; ++ch) {
        ndspChnReset(ch);
        ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
        ndspChnSetRate(ch, kSampleRate);
        ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
        float mix[12] = {0};
        mix[0] = 0.85f;
        mix[1] = 0.85f;
        ndspChnSetMix(ch, mix);
    }

    synthAllClips();
    initialized_ = true;
    return true;
}

void SoundFx::shutdown() {
    if (!initialized_) return;
    ndspChnWaveBufClear(0);
    ndspChnWaveBufClear(1);
    for (int id = 0; id < SFX_COUNT; ++id) {
        if (g_clips[id].samples) {
            linearFree(g_clips[id].samples);
            g_clips[id].samples = nullptr;
            g_clips[id].count   = 0;
        }
    }
    ndspExit();
    initialized_ = false;
}

void SoundFx::play(SoundId id) {
    if (!initialized_ || !enabled_ || id >= SFX_COUNT) return;
    ClipBuffer& cb = g_clips[id];
    if (!cb.samples || cb.count == 0) return;

    int ch = (id == SFX_NAT20 || id == SFX_NAT1) ? 1 : 0;
    ndspChnWaveBufClear(ch);
    std::memset(&cb.waveBuf, 0, sizeof(cb.waveBuf));
    cb.waveBuf.data_vaddr = cb.samples;
    cb.waveBuf.nsamples   = cb.count;
    cb.waveBuf.looping    = false;
    ndspChnWaveBufAdd(ch, &cb.waveBuf);
}

#else

bool SoundFx::init() { return false; }
void SoundFx::shutdown() {}
void SoundFx::play(SoundId) {}

#endif

} // namespace dmtk
