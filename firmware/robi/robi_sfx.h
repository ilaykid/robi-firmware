#ifndef ROBI_SFX_H_
#define ROBI_SFX_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#ifdef ESP_PLATFORM
#include "application.h"
#include "protocol.h"
#endif

namespace robi {

enum class RobiSfxId : uint8_t {
    kCorrectDing = 0,
    kWrongBuzz = 1,
    kSkipSwoosh = 2,
    kCountdownTick = 3,
    kGoStart = 4,
    kVictoryFanfare = 5,
    kDizzyWarble = 6,
    kUiPop = 7,
};

struct SfxNoteSeg {
    float f0_hz;
    float f1_hz;
    int dur_ms;
    float peak_amp;
    float vibrato_hz;
    float vibrato_depth_hz;
};

inline void AppendSynthTone(std::vector<int16_t>& out, const SfxNoteSeg& seg, int sample_rate) {
    if (seg.dur_ms <= 0 || sample_rate <= 0) return;
    const int n = (seg.dur_ms * sample_rate) / 1000;
    if (n <= 0) return;
    const size_t base = out.size();
    out.resize(base + static_cast<size_t>(n));

    const int attack_samples = std::max(1, (5 * sample_rate) / 1000);
    const int release_samples = std::max(1, (18 * sample_rate) / 1000);
    double phase = 0.0;
    constexpr double kTwoPi = 6.283185307179586;

    for (int i = 0; i < n; ++i) {
        float t_norm = static_cast<float>(i) / static_cast<float>(n);
        float t_sec = static_cast<float>(i) / static_cast<float>(sample_rate);
        float freq = seg.f0_hz + (seg.f1_hz - seg.f0_hz) * t_norm;
        if (seg.vibrato_hz > 0.0f && seg.vibrato_depth_hz > 0.0f) {
            freq += seg.vibrato_depth_hz *
                    std::sin(static_cast<float>(kTwoPi) * seg.vibrato_hz * t_sec);
        }
        freq = std::clamp(freq, 80.0f, 6000.0f);

        float env = std::exp(-2.8f * t_norm);
        if (i < attack_samples) {
            env *= static_cast<float>(i) / static_cast<float>(attack_samples);
        }
        if (n - i < release_samples) {
            env *= static_cast<float>(n - i) / static_cast<float>(release_samples);
        }

        // Warm bell/marimba harmonic blend (fundamental + soft 2nd & 3rd overtones)
        double s = 0.76 * std::sin(phase) +
                   0.18 * std::sin(phase * 2.0) +
                   0.06 * std::sin(phase * 3.0);
        phase += kTwoPi * static_cast<double>(freq) / static_cast<double>(sample_rate);
        if (phase > kTwoPi) phase -= kTwoPi;

        int v = static_cast<int>(std::lround(s * static_cast<double>(seg.peak_amp * env)));
        out[base + static_cast<size_t>(i)] = static_cast<int16_t>(std::clamp(v, -28000, 28000));
    }
}

inline void AppendSilenceMs(std::vector<int16_t>& out, int dur_ms, int sample_rate) {
    if (dur_ms <= 0 || sample_rate <= 0) return;
    const int n = (dur_ms * sample_rate) / 1000;
    if (n > 0) {
        out.insert(out.end(), static_cast<size_t>(n), 0);
    }
}

// Pure procedural 16-bit PCM synthesizer (zero flash assets, deterministic, host-testable).
inline std::vector<int16_t> SynthesizeSfxPcm(RobiSfxId id, int sample_rate = 24000) {
    std::vector<int16_t> pcm;
    pcm.reserve(static_cast<size_t>(sample_rate / 2));

    switch (id) {
        case RobiSfxId::kCorrectDing:
            AppendSynthTone(pcm, {1046.5f, 1046.5f, 75, 9200.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {1568.0f, 1568.0f, 165, 10500.0f, 0.0f, 0.0f}, sample_rate);
            break;

        case RobiSfxId::kWrongBuzz:
            AppendSynthTone(pcm, {349.2f, 330.0f, 85, 8200.0f, 0.0f, 0.0f}, sample_rate);
            AppendSilenceMs(pcm, 15, sample_rate);
            AppendSynthTone(pcm, {261.6f, 233.1f, 135, 8200.0f, 0.0f, 0.0f}, sample_rate);
            break;

        case RobiSfxId::kSkipSwoosh:
            AppendSynthTone(pcm, {640.0f, 360.0f, 125, 8600.0f, 0.0f, 0.0f}, sample_rate);
            break;

        case RobiSfxId::kCountdownTick:
            AppendSynthTone(pcm, {880.0f, 880.0f, 55, 7600.0f, 0.0f, 0.0f}, sample_rate);
            break;

        case RobiSfxId::kGoStart:
            AppendSynthTone(pcm, {523.3f, 523.3f, 60, 8800.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {659.3f, 659.3f, 60, 9200.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {784.0f, 784.0f, 65, 9600.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {1046.5f, 1046.5f, 150, 10800.0f, 0.0f, 0.0f}, sample_rate);
            break;

        case RobiSfxId::kVictoryFanfare:
            AppendSynthTone(pcm, {784.0f, 784.0f, 75, 9000.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {1046.5f, 1046.5f, 75, 9500.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {1318.5f, 1318.5f, 85, 10000.0f, 0.0f, 0.0f}, sample_rate);
            AppendSynthTone(pcm, {1568.0f, 1568.0f, 210, 11000.0f, 6.0f, 14.0f}, sample_rate);
            break;

        case RobiSfxId::kDizzyWarble:
            AppendSynthTone(pcm, {740.0f, 480.0f, 360, 8800.0f, 13.5f, 175.0f}, sample_rate);
            break;

        case RobiSfxId::kUiPop:
        default:
            AppendSynthTone(pcm, {720.0f, 960.0f, 38, 6500.0f, 0.0f, 0.0f}, sample_rate);
            break;
    }

    // Pad trailing 20ms digital zero so ES8311 DAC settles cleanly without click
    AppendSilenceMs(pcm, 20, sample_rate);
    return pcm;
}

#ifdef ESP_PLATFORM

inline void PlayRobiSfx(RobiSfxId id, bool allow_interrupt_speaking = false) {
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateSpeaking) {
        if (!allow_interrupt_speaking) {
            return;
        }
        app.AbortSpeaking(kAbortReasonNone);
    }

    std::vector<int16_t> pcm = SynthesizeSfxPcm(id, 24000);
    if (pcm.empty()) return;

    auto& audio = app.GetAudioService();
    constexpr size_t kChunkSamples = 960;  // 40ms @ 24kHz (matches Gemini feeder chunk size)
    size_t offset = 0;
    while (offset < pcm.size()) {
        size_t n = std::min(kChunkSamples, pcm.size() - offset);
        auto pkt = std::make_unique<AudioStreamPacket>();
        pkt->sample_rate = 24000;
        pkt->frame_duration = 0;  // Raw PCM fast-path in OpusCodecTask
        pkt->payload.resize(n * sizeof(int16_t));
        std::memcpy(pkt->payload.data(), pcm.data() + offset, pkt->payload.size());
        if (!audio.PushPacketToDecodeQueue(std::move(pkt), false)) {
            break;
        }
        offset += n;
    }
}

#endif  // ESP_PLATFORM

}  // namespace robi

#endif  // ROBI_SFX_H_
