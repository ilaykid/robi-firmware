#include "../firmware/robi/robi_audio_sync.h"
#include "../firmware/robi/robi_face_math.h"
#include "../firmware/robi/robi_mdns.h"
#include "../firmware/robi/robi_utf8.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

bool IsStrictlyValidUtf8(std::string_view s) {
    size_t pos = 0;
    while (pos < s.size()) {
        size_t n = robi::Utf8ValidCodepointLen(s, pos);
        if (n == 0) return false;
        pos += n;
    }
    return true;
}

void TestUtf8SafeTruncateAndSanitize() {
    const std::string sample = "😄 שלום רובי! בוא נשחק חידון חלל ודינוזאורים 🚀🌟 עם נועה בת 8!";
    assert(IsStrictlyValidUtf8(sample));

    // Truncate at EVERY possible byte boundary 0..sample.size()+5 and verify strict RFC 3629 UTF-8
    for (size_t max_b = 0; max_b <= sample.size() + 5; ++max_b) {
        std::string cut = robi::Utf8SafeTruncate(sample, max_b);
        assert(cut.size() <= max_b);
        assert(IsStrictlyValidUtf8(cut));
    }

    // Corrupt string with dangling Hebrew lead byte 0xD7 at end (exact bug from substr(0, 240))
    std::string broken = "תשובה בעברית";
    broken.push_back(static_cast<char>(0xD7));
    assert(!IsStrictlyValidUtf8(broken));
    std::string healed = robi::Utf8Sanitize(broken);
    assert(healed == "תשובה בעברית");
    assert(IsStrictlyValidUtf8(healed));

    // Reject overlong 2-byte (0xC0 0xAF) and UTF-16 surrogate (0xED 0xA0 0x80)
    std::string malicious = {'A', static_cast<char>(0xC0), static_cast<char>(0xAF),
                             static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80), 'B'};
    assert(robi::Utf8Sanitize(malicious) == "AB");
}

void TestDcRemovalAndVoiceLevel() {
    constexpr size_t kN = 512;
    std::vector<int16_t> pcm(kN);
    for (size_t i = 0; i < kN; ++i) {
        double s = 1200.0 * std::sin(2.0 * 3.141592653589793 * static_cast<double>(i) / 32.0);
        pcm[i] = static_cast<int16_t>(285 + static_cast<int>(std::lround(s)));  // ES7210 +285 DC offset
    }

    float rms = robi::RemoveDcAndComputeRms(pcm.data(), pcm.size());
    // Theoretical RMS of 1200 amplitude sine is 1200 / sqrt(2) ~= 848.5
    assert(rms > 835.0f && rms < 862.0f);

    int64_t post_sum = 0;
    for (int16_t v : pcm) post_sum += v;
    assert(std::abs(post_sum) < static_cast<int64_t>(kN));  // Zero-mean within +/-1 LSB

    std::vector<int16_t> quiet(512, 30);
    assert(robi::ComputeVoiceLevel0To100(quiet.data(), quiet.size()) == 0);
    int mid_lvl = robi::ComputeVoiceLevel0To100(pcm.data(), pcm.size());
    assert(mid_lvl > 15 && mid_lvl < 80);

    std::vector<int16_t> loud(512, 6000);
    assert(robi::ComputeVoiceLevel0To100(loud.data(), loud.size()) == 100);
}

void TestSampleSyncedEmotionRing() {
    std::vector<int16_t> backing(32768, 0);
    robi::SampleSyncedJitterRing ring;
    ring.AttachBuffer(backing.data(), backing.size());

    std::atomic<bool> stopping{false};
    std::atomic<bool> drop_turn{false};
    std::vector<int16_t> chunk(6000, 42);

    // Clause 1 at sample 0: "happy"
    ring.QueueEmotionAtWriteCursor("happy");
    ring.PushSamples(chunk.data(), 6000, stopping, drop_turn);   // 0..6000
    ring.PushSamples(chunk.data(), 6000, stopping, drop_turn);   // 6000..12000

    // Clause 2 at sample 12000: "surprised"
    ring.QueueEmotionAtWriteCursor("surprised");
    ring.PushSamples(chunk.data(), 6000, stopping, drop_turn);   // 12000..18000

    // Clause 3 at sample 18000: "winking"
    ring.QueueEmotionAtWriteCursor("winking");
    ring.PushSamples(chunk.data(), 6000, stopping, drop_turn);   // 18000..24000

    std::string emo;
    // At popped == 0: ONLY "happy" is due! "surprised" (12000) must NOT fire early!
    assert(ring.PopDueEmotion(false, emo) && emo == "happy");
    assert(!ring.PopDueEmotion(false, emo));

    std::vector<int16_t> out(1200);
    for (int f = 0; f < 9; ++f) {  // Pop 9 * 1200 = 10800 samples (< 12000)
        assert(ring.PopSamples(out.data(), 1200) == 1200);
        assert(!ring.PopDueEmotion(false, emo));
    }

    // Pop 10th frame -> popped_samples == 12000 -> "surprised" fires on exact boundary!
    assert(ring.PopSamples(out.data(), 1200) == 1200);
    assert(ring.PopDueEmotion(false, emo) && emo == "surprised");
    assert(!ring.PopDueEmotion(false, emo));

    // Pop 5 more frames -> popped_samples == 18000 -> "winking" fires on exact boundary!
    for (int f = 0; f < 4; ++f) {
        assert(ring.PopSamples(out.data(), 1200) == 1200);
        assert(!ring.PopDueEmotion(false, emo));
    }
    assert(ring.PopSamples(out.data(), 1200) == 1200);
    assert(ring.PopDueEmotion(false, emo) && emo == "winking");
}

void TestConcurrentRingWrapAround() {
    std::vector<int16_t> backing(8192, 0);
    auto ring_ptr = std::make_unique<robi::SampleSyncedJitterRing>();
    auto& ring = *ring_ptr;
    ring.AttachBuffer(backing.data(), backing.size());

    std::atomic<bool> stopping{false};
    std::atomic<bool> drop_turn{false};
    constexpr size_t kTotalSamples = 96000;  // ~12 full ring wrap-arounds

    std::thread producer([&]() {
        std::vector<int16_t> in_chunk(480);
        size_t sent = 0;
        while (sent < kTotalSamples) {
            size_t n = std::min<size_t>(480, kTotalSamples - sent);
            for (size_t i = 0; i < n; ++i) {
                in_chunk[i] = static_cast<int16_t>((sent + i) & 0x7FFFu);
            }
            if (sent % 24000 == 0) {
                ring.QueueEmotionAtWriteCursor("cool");
            }
            ring.PushSamples(in_chunk.data(), n, stopping, drop_turn);
            sent += n;
        }
    });

    size_t received = 0;
    std::vector<int16_t> out_chunk(1200);
    while (received < kTotalSamples) {
        size_t got = ring.PopSamples(out_chunk.data(), out_chunk.size());
        if (got == 0) {
            ring.WaitFor(std::chrono::milliseconds(5), [&](size_t c) { return c > 0; });
            continue;
        }
        ring.NotifyOne();
        for (size_t i = 0; i < got; ++i) {
            int16_t expected = static_cast<int16_t>((received + i) & 0x7FFFu);
            assert(out_chunk[i] == expected);
        }
        received += got;
    }

    producer.join();
    assert(ring.written_samples() == kTotalSamples);
    assert(ring.popped_samples() == kTotalSamples);
}

void TestAdaptiveSpeechTracker() {
    robi::AdaptiveSpeechTracker tracker;

    // 1. Brief 64ms click should never trigger soft stream end
    for (int i = 0; i < 2; ++i) {
        auto d = tracker.OnFrame(600.0f, false, 32);
        assert(d.active_voice && !d.should_send_soft_stream_end);
    }
    for (int i = 0; i < 50; ++i) {
        auto d = tracker.OnFrame(60.0f, false, 32);
        assert(!d.should_send_soft_stream_end);
    }

    // 2. Real speech (12 * 32ms = 384ms) followed by noisy-room hum (120 RMS) triggers soft flush once
    for (int i = 0; i < 12; ++i) {
        auto d = tracker.OnFrame(900.0f, true, 32);
        assert(d.active_voice && !d.should_send_soft_stream_end);
    }
    int flush_count = 0;
    for (int i = 0; i < 60; ++i) {
        auto d = tracker.OnFrame(120.0f, false, 32);
        if (d.should_send_soft_stream_end) ++flush_count;
    }
    assert(flush_count == 1);
}

void TestThemeColorPreservationAcrossAllEmotions() {
    const char* emotions[] = {
        "neutral", "listening", "thinking", "happy", "laughing", "loving", "kissy",
        "surprised", "shocked", "winking", "cool", "confident", "confused", "silly",
        "embarrassed", "relaxed", "sad", "crying", "angry", "sleepy",
    };
    const robi::RobiThemeId themes[] = {
        robi::ROBI_THEME_CYBER_CYAN,
        robi::ROBI_THEME_SOLAR_GOLD,
        robi::ROBI_THEME_EMERALD_MATRIX,
        robi::ROBI_THEME_COSMIC_VIOLET,
        robi::ROBI_THEME_CORAL_CUTE,
    };
    const robi::RobiFaceStyle styles[] = {
        robi::ROBI_STYLE_CYBER_VISOR,
        robi::ROBI_STYLE_ROUND_KAWAII,
        robi::ROBI_STYLE_RETRO_PIXEL,
        robi::ROBI_STYLE_VISOR_CYCLOPS,
        robi::ROBI_STYLE_OWL_MECH,
    };

    for (auto th : themes) {
        uint32_t expected_hex = robi::RobiThemeBaseHex(th);
        for (auto st : styles) {
            for (const char* emo : emotions) {
                auto res = robi::RobiComputePresetParams(emo, th, st);
                uint32_t actual_hex = robi::RobiPackRgb(
                    res.params.col_r, res.params.col_g, res.params.col_b);
                // Strict invariant: NO emotion may ever hijack the user's chosen theme color!
                assert(actual_hex == expected_hex);
            }
        }
    }
}

void TestMdnsParserAndNsecBuilder() {
    // Query for "RoBi.LoCaL" AAAA (qtype = 28)
    const uint8_t query_aaaa[] = {
        0x12, 0x34, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        4, 'R', 'o', 'B', 'i', 5, 'L', 'o', 'C', 'a', 'L', 0,
        0x00, 28, 0x00, 0x01,
    };
    uint16_t qtype = 0;
    assert(robi::ParseMdnsRobiLocalQuery(query_aaaa, sizeof(query_aaaa), qtype));
    assert(qtype == 28);

    uint8_t resp[64] = {};
    const uint8_t ip[4] = {192, 168, 1, 77};
    size_t rlen = robi::BuildMdnsRobiLocalPacket(resp, 0x1234, ip, qtype);
    assert(rlen == 55);
    assert(resp[6] == 0x00 && resp[7] == 0x02);  // ANCOUNT == 2 (A + NSEC) for AAAA

    // Malicious self-referencing compression pointer loop at offset 12 (0xC0 0x0C) must terminate safely
    const uint8_t loop_pkt[] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    };
    assert(!robi::ParseMdnsRobiLocalQuery(loop_pkt, sizeof(loop_pkt), qtype));
}

}  // namespace

int main() {
    TestUtf8SafeTruncateAndSanitize();
    TestDcRemovalAndVoiceLevel();
    TestSampleSyncedEmotionRing();
    TestConcurrentRingWrapAround();
    TestAdaptiveSpeechTracker();
    TestThemeColorPreservationAcrossAllEmotions();
    TestMdnsParserAndNsecBuilder();
    std::printf("ALL ROBI SYNC & LATENCY UNIT TESTS PASSED!\n");
    return 0;
}
