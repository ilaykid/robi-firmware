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
    bool active_voice = false;
    bool should_send_soft_stream_end = false;
};

// Non-blocking adaptive speech tracker:
// Never gates or blocks continuous 16kHz PCM streaming to Gemini's neural VAD.
// Only emits a soft one-shot `audioStreamEnd` backstop if the user clearly spoke (>= 192ms)
// and then stayed quiet for >= 1050ms while ambient room hum prevented server VAD from closing.
class AdaptiveSpeechTracker {
public:
    void Reset() {
        speech_accum_ms_ = 0;
        post_speech_quiet_ms_ = 0;
        soft_flush_sent_ = false;
    }

    SpeechFrameDecision OnFrame(float ac_rms, bool afe_vad, uint32_t frame_ms) {
        SpeechFrameDecision dec;
        const float speech_thresh = std::clamp(noise_floor_rms_ * 2.5f + 70.0f, 260.0f, 950.0f);
        const float quiet_thresh = std::clamp(noise_floor_rms_ * 1.7f + 45.0f, 190.0f, 650.0f);

        dec.active_voice = afe_vad || (ac_rms >= speech_thresh);

        if (!afe_vad && ac_rms < speech_thresh * 0.85f) {
            float sample_floor = std::clamp(ac_rms, 25.0f, 420.0f);
            noise_floor_rms_ = 0.96f * noise_floor_rms_ + 0.04f * sample_floor;
        }

        if (dec.active_voice) {
            speech_accum_ms_ = std::min<uint32_t>(speech_accum_ms_ + frame_ms, 30000u);
            post_speech_quiet_ms_ = 0;
            soft_flush_sent_ = false;
            return dec;
        }

        if (speech_accum_ms_ >= 192u) {
            post_speech_quiet_ms_ += (ac_rms <= quiet_thresh) ? frame_ms : (frame_ms / 2u);
            if (!soft_flush_sent_ && post_speech_quiet_ms_ >= 1050u) {
                soft_flush_sent_ = true;
                speech_accum_ms_ = 0;
                post_speech_quiet_ms_ = 0;
                dec.should_send_soft_stream_end = true;
            }
        } else if (speech_accum_ms_ > 0) {
            post_speech_quiet_ms_ += frame_ms;
            if (post_speech_quiet_ms_ >= 320u) {
                speech_accum_ms_ = 0;
                post_speech_quiet_ms_ = 0;
            }
        }
        return dec;
    }

    float noise_floor_rms() const { return noise_floor_rms_; }

private:
    float noise_floor_rms_ = 75.0f;
    uint32_t speech_accum_ms_ = 0;
    uint32_t post_speech_quiet_ms_ = 0;
    bool soft_flush_sent_ = false;
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
