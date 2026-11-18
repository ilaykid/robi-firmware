#ifndef ROBI_AUDIO_SYNC_H_
#define ROBI_AUDIO_SYNC_H_

#include "robi_utf8.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>

namespace robi {

struct EmojiMapping {
    const char* utf8;
    const char* emotion;
};

inline constexpr EmojiMapping kEmojiMap[] = {
    {"😂", "laughing"},    {"🤣", "laughing"},    {"😆", "laughing"},
    {"🤩", "surprised"},   {"😮", "surprised"},   {"😯", "surprised"},   {"🌟", "surprised"},
    {"😱", "shocked"},     {"🤯", "shocked"},     {"⚡", "shocked"},
    {"🥰", "loving"},      {"😍", "loving"},      {"❤", "loving"},       {"💖", "loving"},    {"🤗", "loving"},
    {"😘", "kissy"},       {"😗", "kissy"},
    {"😉", "winking"},     {"😜", "winking"},     {"😝", "winking"},
    {"😎", "cool"},        {"🤖", "cool"},        {"🚀", "cool"},
    {"🤔", "thinking"},    {"🧐", "thinking"},    {"💭", "thinking"},
    {"😕", "confused"},    {"🤨", "confused"},
    {"🤪", "silly"},       {"😛", "silly"},
    {"😳", "embarrassed"}, {"🙈", "embarrassed"},
    {"😌", "relaxed"},     {"😋", "relaxed"},
    {"😢", "sad"},         {"😔", "sad"},
    {"😭", "crying"},
    {"😠", "angry"},       {"😡", "angry"},
    {"😴", "sleepy"},      {"🥱", "sleepy"},
    {"😏", "confident"},   {"💪", "confident"},
    {"😄", "happy"},       {"😊", "happy"},       {"🙂", "happy"},       {"😁", "happy"},     {"🎉", "happy"},
};

inline const char* MatchLeadingOrInlineEmoji(std::string_view s) {
    for (const auto& m : kEmojiMap) {
        if (s.find(m.utf8) != std::string_view::npos) {
            return m.emotion;
        }
    }
    return nullptr;
}

inline const char* InferHebrewClauseEmotion(std::string_view clause) {
    if (clause.size() < 12) return nullptr;
    auto has = [&](const char* kw) { return clause.find(kw) != std::string_view::npos; };

    if (has("מטורף") || has("לא ייאמן") || has("פיצוץ") || has("טריליון")) return "shocked";
    if (has("מדהים") || has("וואו") || has("הידעת") || has("מפתיע") || has("סופרנובה") || has("קסם"))
        return "surprised";
    if (has("מצחיק") || has("בדיחה") || has("חחח") || has("משוגע")) return "laughing";
    if (has("רוצה לנחש") || has("טריק") || has("קריצה")) return "winking";
    if (has("תחשוב") || has("דמיין") || has("מעניין") || has("שאלה מצוינת") || has("חידה"))
        return "thinking";
    if (has("כל הכבוד") || has("אלוף") || has("פגעת בול") || has("נכון מאוד") || has("גאה בך"))
        return "loving";
    if (has("רובוט") || has("חללית") || has("טכנולוגיה") || has("לייזר") || has("מגניב"))
        return "cool";
    if (has("מעולה") || has("נפלא") || has("יאללה") || has("בטח")) return "happy";
    return nullptr;
}

inline std::string CleanTranscriptText(std::string_view raw) {
    size_t b = 0;
    while (b < raw.size() && (raw[b] == ' ' || raw[b] == '\t' || raw[b] == '\r' || raw[b] == '\n')) {
        ++b;
    }
    size_t e = raw.size();
    while (e > b && (raw[e - 1] == ' ' || raw[e - 1] == '\t' || raw[e - 1] == '\r' || raw[e - 1] == '\n')) {
        --e;
    }
    if (e <= b) return {};
    std::string trimmed = Utf8Sanitize(raw.substr(b, e - b));
    if (trimmed.empty()) return {};

    std::string low;
    low.reserve(trimmed.size());
    for (unsigned char c : trimmed) {
        low.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c));
    }
    if (low == "<noise>" || low == "[noise]" || low == "(noise)" ||
        low == "<silence>" || low == "[silence]" || low == "...") {
        return {};
    }
    return trimmed;
}

inline bool HasMeaningfulSpeechText(std::string_view cleaned) {
    if (cleaned.empty()) return false;
    int letters = 0;
    int digits = 0;
    for (unsigned char c : cleaned) {
        if (c >= '0' && c <= '9') {
            ++digits;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c & 0xC0u) == 0xC0u) {
            ++letters;
        }
    }
    return digits >= 1 || letters >= 2;
}

inline bool IsWakeWordToken(std::string_view text) {
    std::string low;
    low.reserve(text.size());
    for (unsigned char c : text) {
        low.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c));
    }
    return low == "hey robby" || low == "hey robi" || low == "hi,esp" ||
           low == "hi esp" || low == "hiesp";
}

// Fast exact integer DC-bias removal + single hardware-FPU float sqrtf for ESP32-S3
// (avoids 1,000+ software-emulated 64-bit double ops per 32ms frame on main loop).
inline float RemoveDcAndComputeRms(int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0) return 0.0f;

    int64_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += samples[i];
    }
    const int32_t dc_offset = static_cast<int32_t>(
        (sum >= 0) ? ((sum + static_cast<int64_t>(count / 2)) / static_cast<int64_t>(count))
                   : ((sum - static_cast<int64_t>(count / 2)) / static_cast<int64_t>(count)));

    uint64_t sum_sq = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t v = std::clamp<int32_t>(static_cast<int32_t>(samples[i]) - dc_offset, -32768, 32767);
        samples[i] = static_cast<int16_t>(v);
        int64_t lv = v;
        sum_sq += static_cast<uint64_t>(lv * lv);
    }

    float mean_sq = static_cast<float>(sum_sq) / static_cast<float>(count);
    return std::sqrt(mean_sq);
}

inline int ComputeVoiceLevel0To100(const int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0) return 0;
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t v = samples[i];
        sum_sq += static_cast<uint64_t>(v * v);
    }
    float rms = std::sqrt(static_cast<float>(sum_sq) / static_cast<float>(count));
    if (rms < 105.0f) return 0;
    float norm = std::min(1.0f, (rms - 95.0f) / 4100.0f);
    // Fast perceptual curve (~norm^0.72) using sqrtf blend instead of double std::pow
    float curved = 0.62f * std::sqrt(norm) + 0.38f * norm;
    int lvl = static_cast<int>(curved * 100.0f + 0.5f);
    return std::clamp(lvl, 0, 100);
}

struct SpeechFrameDecision {
    bool start_utterance = false;
    bool in_speech = false;
    bool commit_utterance = false;
};

// Smooth, non-clipping speech pre-gain for 3D-printed enclosures where AFE has AGC disabled:
// boosts quiet/distant human speech (e.g. 150..500 RMS) up toward ~2100 RMS (-23.8 dBFS) so
// Google Gemini Live's server-side VAD and Hebrew STT recognize soft voices effortlessly,
// while leaving loud close-talk speech (>= 2100 RMS) at unity gain (1.0x).
inline float NormalizeSpeechPcm(int16_t* samples, size_t count, float ac_rms, float& smooth_gain) {
    if (samples == nullptr || count == 0) return smooth_gain;
    float target_gain = std::clamp(2100.0f / std::max(ac_rms, 160.0f), 1.0f, 4.6f);
    smooth_gain = 0.65f * smooth_gain + 0.35f * target_gain;
    if (smooth_gain <= 1.03f) {
        return smooth_gain;
    }
    const int32_t scale_q10 = static_cast<int32_t>(smooth_gain * 1024.0f + 0.5f);
    for (size_t i = 0; i < count; ++i) {
        int32_t v = (static_cast<int32_t>(samples[i]) * scale_q10 + 512) >> 10;
        samples[i] = static_cast<int16_t>(std::clamp<int32_t>(v, -30000, 30000));
    }
    return smooth_gain;
}

// Hardware-calibrated adaptive utterance tracker for Waveshare ESP32-S3 ES7210 (+30dB ADC, no NSNet/AGC).
// - Completely independent of ESP-SR VAD_MODE_0 (which stays latched true on ES7210 +30dB self-noise).
// - Calibrates immediately on Frame #1 via minimum-statistics seeding so quiet enclosures (~45..85 RMS)
//   trigger speech onset at >= 145 RMS without waiting, while louder board hiss (~250 RMS) never false-triggers.
// - Prevents speech self-poisoning: upward floor tracking only runs within 1.45x of the current floor
//   and is strictly capped at 265 RMS so continuous speech can never lock out onset detection.
class AdaptiveSpeechTracker {
public:
    void ResetSession() {
        calibrated_ = false;
        noise_floor_rms_ = 95.0f;
        ResetTurn();
    }

    void ResetTurn() {
        in_speech_ = false;
        onset_ms_ = 0;
        speech_ms_ = 0;
        silence_ms_ = 0;
        peak_rms_ = 0.0f;
    }

    void Reset() { ResetTurn(); }

    SpeechFrameDecision OnFrame(float ac_rms, uint32_t frame_ms) {
        SpeechFrameDecision dec;
        frame_ms = std::clamp<uint32_t>(frame_ms, 10u, 120u);

        if (!calibrated_) {
            calibrated_ = true;
            if (ac_rms <= 310.0f) {
                noise_floor_rms_ = std::clamp(ac_rms, 15.0f, 260.0f);
            } else {
                // User is already speaking on Frame #1 right after OGG_POPUP
                noise_floor_rms_ = 120.0f;
            }
        }

        if (!in_speech_) {
            const float onset_thresh = std::clamp(noise_floor_rms_ * 1.75f + 35.0f, 65.0f, 760.0f);
            if (ac_rms >= onset_thresh) {
                onset_ms_ += frame_ms;
                if (onset_ms_ >= 120u) {
                    in_speech_ = true;
                    speech_ms_ = onset_ms_;
                    silence_ms_ = 0;
                    peak_rms_ = ac_rms;
                    onset_ms_ = 0;
                    dec.start_utterance = true;
                    dec.in_speech = true;
                }
                return dec;
            }

            onset_ms_ = (onset_ms_ > (frame_ms / 2u)) ? (onset_ms_ - (frame_ms / 2u)) : 0u;
            if (ac_rms < noise_floor_rms_) {
                float sample = std::max(15.0f, ac_rms);
                noise_floor_rms_ = 0.68f * noise_floor_rms_ + 0.32f * sample;
            } else if (ac_rms < noise_floor_rms_ * 1.45f + 30.0f) {
                float sample = std::min(265.0f, ac_rms);
                noise_floor_rms_ = 0.96f * noise_floor_rms_ + 0.04f * sample;
            }
            return dec;
        }

        // Active speech utterance in progress
        dec.in_speech = true;
        peak_rms_ = std::max(peak_rms_, ac_rms);
        speech_ms_ = std::min<uint32_t>(speech_ms_ + frame_ms, 30000u);

        const float floor_quiet = std::clamp(noise_floor_rms_ * 1.45f + 32.0f, 48.0f, 430.0f);
        const float peak_drop_quiet = std::clamp(peak_rms_ * 0.26f, 48.0f, 480.0f);
        const float quiet_thresh = std::max(floor_quiet, peak_drop_quiet);

        if (ac_rms < quiet_thresh) {
            silence_ms_ += frame_ms;
            if (silence_ms_ >= 600u) {
                dec.commit_utterance = true;
                ResetTurn();
            }
        } else {
            if (ac_rms >= quiet_thresh * 1.30f) {
                silence_ms_ = 0;
            } else {
                silence_ms_ = (silence_ms_ > frame_ms) ? (silence_ms_ - frame_ms) : 0u;
            }
            if (speech_ms_ >= 8000u) {
                dec.commit_utterance = true;
                ResetTurn();
            }
        }
        return dec;
    }

    float noise_floor_rms() const { return noise_floor_rms_; }
    float current_onset_thresh() const {
        return std::clamp(noise_floor_rms_ * 1.75f + 35.0f, 65.0f, 760.0f);
    }
    bool in_speech() const { return in_speech_; }
    uint32_t speech_ms() const { return speech_ms_; }

private:
    bool calibrated_ = false;
    float noise_floor_rms_ = 95.0f;
    bool in_speech_ = false;
    uint32_t onset_ms_ = 0;
    uint32_t speech_ms_ = 0;
    uint32_t silence_ms_ = 0;
    float peak_rms_ = 0.0f;
};

struct ScheduledEmotion {
    uint64_t trigger_sample = 0;
    std::string emotion;
};

// Sample-accurate PSRAM jitter ring buffer that synchronizes queued facial emotions
// to the exact 24kHz PCM sample position where each clause enters the audio stream.
class SampleSyncedJitterRing {
public:
    void AttachBuffer(int16_t* backing, size_t capacity_samples) {
        std::lock_guard<std::mutex> lk(mutex_);
        buf_ = backing;
        capacity_ = (backing != nullptr) ? capacity_samples : 0;
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        written_samples_ = 0;
        popped_samples_ = 0;
        emotions_.clear();
    }

    size_t capacity() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return capacity_;
    }

    size_t Available() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return count_;
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mutex_);
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        written_samples_ = 0;
        popped_samples_ = 0;
        emotions_.clear();
    }

    void NotifyAll() { cv_.notify_all(); }
    void NotifyOne() { cv_.notify_one(); }

    void PushSamples(const int16_t* samples, size_t count,
                     const std::atomic<bool>& stopping,
                     const std::atomic<bool>& drop_turn) {
        if (samples == nullptr || count == 0) return;
        std::unique_lock<std::mutex> lk(mutex_);
        if (buf_ == nullptr || capacity_ == 0) return;
        for (size_t i = 0; i < count; ++i) {
            if (count_ == capacity_) {
                cv_.wait_for(lk, std::chrono::milliseconds(60), [&]() {
                    return stopping.load() || drop_turn.load() || count_ < capacity_;
                });
                if (stopping.load() || drop_turn.load()) return;
                if (count_ == capacity_) {
                    // Advance oldest sample and popped cursor together so emotion offsets stay aligned
                    head_ = (head_ + 1) % capacity_;
                    --count_;
                    ++popped_samples_;
                }
            }
            buf_[tail_] = samples[i];
            tail_ = (tail_ + 1) % capacity_;
            ++count_;
            ++written_samples_;
        }
        lk.unlock();
        cv_.notify_one();
    }

    size_t PopSamples(int16_t* out_samples, size_t max_count) {
        if (out_samples == nullptr || max_count == 0) return 0;
        std::lock_guard<std::mutex> lk(mutex_);
        if (buf_ == nullptr || capacity_ == 0) return 0;
        size_t n = std::min(max_count, count_);
        for (size_t i = 0; i < n; ++i) {
            out_samples[i] = buf_[head_];
            head_ = (head_ + 1) % capacity_;
        }
        count_ -= n;
        popped_samples_ += n;
        return n;
    }

    void QueueEmotionAtWriteCursor(std::string emotion) {
        if (emotion.empty()) return;
        std::lock_guard<std::mutex> lk(mutex_);
        if (!emotions_.empty() && emotions_.back().emotion == emotion) {
            return;
        }
        if (emotions_.size() >= 24) {
            emotions_.pop_front();
        }
        emotions_.push_back({written_samples_, std::move(emotion)});
    }

    bool PopDueEmotion(bool flush_all, std::string& out_emotion) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (emotions_.empty()) return false;
        if (!flush_all && emotions_.front().trigger_sample > popped_samples_) {
            return false;
        }
        out_emotion = std::move(emotions_.front().emotion);
        emotions_.pop_front();
        return true;
    }

    template <typename Pred>
    void WaitFor(std::chrono::milliseconds max_wait, Pred&& pred) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait_for(lk, max_wait, [&]() { return pred(count_); });
    }

    uint64_t written_samples() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return written_samples_;
    }

    uint64_t popped_samples() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return popped_samples_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int16_t* buf_ = nullptr;
    size_t capacity_ = 0;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint64_t written_samples_ = 0;
    uint64_t popped_samples_ = 0;
    std::deque<ScheduledEmotion> emotions_;
};

}  // namespace robi

#endif  // ROBI_AUDIO_SYNC_H_
