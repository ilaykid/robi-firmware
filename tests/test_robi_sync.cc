#include "../firmware/robi/robi_audio_sync.h"
#include "../firmware/robi/robi_face_math.h"
#include "../firmware/robi/robi_games.h"
#include "../firmware/robi/robi_imu.h"
#include "../firmware/robi/robi_mdns.h"
#include "../firmware/robi/robi_sfx.h"
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
    // 1. Waveshare ES7210 +30dB idle self-noise (~250..295 RMS on 60ms AFE frames) must NEVER false-trigger
    robi::AdaptiveSpeechTracker tracker;
    tracker.ResetSession();
    for (int i = 0; i < 100; ++i) {
        float idle_rms = (i % 5 == 0) ? 295.0f : 250.0f;
        auto d = tracker.OnFrame(idle_rms, 60);
        assert(!d.start_utterance && !d.in_speech && !d.commit_utterance);
    }

    // 2. Single 60ms click/pop (700 RMS) must NOT trigger speech onset
    {
        auto d1 = tracker.OnFrame(700.0f, 60);
        assert(!d1.start_utterance && !d1.in_speech);
        for (int i = 0; i < 20; ++i) {
            auto d2 = tracker.OnFrame(250.0f, 60);
            assert(!d2.start_utterance && !d2.in_speech && !d2.commit_utterance);
        }
    }

    // 3. Normal speech with mid-sentence 300ms pause, returning to 260 RMS board noise -> commits at 600ms!
    {
        auto f1 = tracker.OnFrame(1600.0f, 60);  // 60ms (< 120ms)
        assert(!f1.start_utterance && !f1.in_speech);
        auto f2 = tracker.OnFrame(1800.0f, 60);  // 120ms -> ONSET!
        assert(f2.start_utterance && f2.in_speech && !f2.commit_utterance);

        for (int i = 0; i < 6; ++i) {
            auto d = tracker.OnFrame(1500.0f, 60);
            assert(d.in_speech && !d.commit_utterance);
        }
        // 300ms word pause (5 * 60ms < 600ms) must NOT prematurely cut off the child
        for (int i = 0; i < 5; ++i) {
            auto d = tracker.OnFrame(260.0f, 60);
            assert(d.in_speech && !d.commit_utterance);
        }
        // Second word resumes -> resets silence accumulator cleanly
        for (int i = 0; i < 5; ++i) {
            auto d = tracker.OnFrame(1700.0f, 60);
            assert(d.in_speech && !d.commit_utterance);
        }
        // Post-speech return to 260 RMS board noise -> commits on 10th frame (600ms)
        int commits = 0;
        for (int i = 0; i < 10; ++i) {
            auto d = tracker.OnFrame(260.0f, 60);
            if (i < 9) {
                assert(d.in_speech && !d.commit_utterance);
            } else {
                assert(d.commit_utterance);
                ++commits;
            }
        }
        assert(commits == 1);
        assert(!tracker.in_speech());
    }

    // 4. Real Waveshare 3D-enclosure telemetry (9..15 RMS ambient, 921 RMS speech) + soft 85 RMS speech
    {
        tracker.ResetSession();
        for (int i = 0; i < 6; ++i) {
            auto idle = tracker.OnFrame((i & 1) ? 15.0f : 9.0f, 60);
            assert(!idle.in_speech && !idle.start_utterance);
        }
        assert(tracker.noise_floor_rms() <= 16.0f);
        assert(tracker.current_onset_thresh() <= 70.0f);

        // Even soft speech (85 RMS) triggers onset in 2 frames (120ms) and commits cleanly at 11 RMS
        auto w1 = tracker.OnFrame(85.0f, 60);
        assert(!w1.start_utterance);
        auto w2 = tracker.OnFrame(95.0f, 60);
        assert(w2.start_utterance && w2.in_speech);

        for (int i = 0; i < 6; ++i) {
            auto d = tracker.OnFrame(140.0f, 60);
            assert(d.in_speech && !d.commit_utterance);
        }
        bool committed = false;
        for (int i = 0; i < 10; ++i) {
            if (tracker.OnFrame(11.0f, 60).commit_utterance) committed = true;
        }
        assert(committed);
    }

    // 5. Immediate speech on Frame #1 right after OGG_POPUP (360 RMS) triggers on Frame #2 (120ms)
    {
        tracker.ResetSession();
        auto s1 = tracker.OnFrame(360.0f, 60);
        assert(!s1.start_utterance);
        auto s2 = tracker.OnFrame(380.0f, 60);
        assert(s2.start_utterance && s2.in_speech);
    }

    // 6. Speech pre-gain normalizer boosts soft 200 RMS speech cleanly and leaves 2400 RMS untouched
    {
        std::vector<int16_t> quiet(160, 200);
        for (size_t i = 0; i < quiet.size(); ++i) {
            quiet[i] = (i & 1) ? 200 : -200;
        }
        float gain = 1.0f;
        for (int f = 0; f < 6; ++f) {
            std::fill(quiet.begin(), quiet.end(), static_cast<int16_t>(200));
            for (size_t i = 0; i < quiet.size(); ++i) quiet[i] = (i & 1) ? 200 : -200;
            robi::NormalizeSpeechPcm(quiet.data(), quiet.size(), 200.0f, gain);
        }
        assert(gain > 3.5f && gain <= 4.6f);
        assert(std::abs(quiet[0]) >= 700 && std::abs(quiet[0]) <= 1000);

        float loud_gain = 1.0f;
        std::vector<int16_t> loud(160, 2400);
        robi::NormalizeSpeechPcm(loud.data(), loud.size(), 2400.0f, loud_gain);
        assert(loud_gain <= 1.01f);
        assert(loud[0] == 2400);
    }

    // 7. Extreme continuous background shouting (1800 RMS non-stop) hits 8.0s safety cap instead of hanging forever
    {
        tracker.ResetSession();
        int cap_commits = 0;
        for (int i = 0; i < 145; ++i) {  // 145 * 60ms = 8.7s
            if (tracker.OnFrame(1800.0f, 60).commit_utterance) ++cap_commits;
        }
        assert(cap_commits == 1);
    }
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

void TestImuGesturesAndForeheadTilt() {
    robi::ImuGestureTracker trk;
    trk.Reset();
    trk.SetZFrontSign(-1.0f);  // Test inverted back-mounted PCB sign (`az = -1g` when screen faces sky)

    // 1. Desk bump / speaker bass vibration WITHOUT gyro rotation must NEVER false-trigger dizzy!
    for (int i = 0; i < 30; ++i) {
        robi::ImuSample bump = {(i % 2 == 0) ? 1.9f : -1.9f, 0.0f, -0.95f, 15.0f, 10.0f, 5.0f};
        auto ev = trk.Step(bump, 40);
        assert(!ev.shake_dizzy && !ev.enter_face_down_sleep);
    }

    // 2. Deliberate in-hand shake (alternating linear g + >165 dps gyro) triggers shake_dizzy once!
    bool got_dizzy = false;
    for (int i = 0; i < 8; ++i) {
        float dir = (i % 2 == 0) ? 1.0f : -1.0f;
        robi::ImuSample shake = {2.1f * dir, 0.2f, -0.3f, 0.0f, 220.0f * dir, 90.0f};
        auto ev = trk.Step(shake, 40);
        if (ev.shake_dizzy) {
            assert(!got_dizzy);  // Cooldown prevents immediate duplicate trigger
            got_dizzy = true;
        }
    }
    assert(got_dizzy);

    // 3. Flipping flat face-down (`az = +0.96g` -> `screen_z = -0.96g`) enters sleep after 650ms,
    //    and lifting back up exits sleep cleanly!
    bool entered_sleep = false;
    for (int i = 0; i < 20; ++i) {
        robi::ImuSample face_down = {0.05f, -0.04f, 0.96f, 2.0f, 1.0f, 0.0f};
        auto ev = trk.Step(face_down, 40);
        if (ev.enter_face_down_sleep) entered_sleep = true;
    }
    assert(entered_sleep && trk.is_face_down());

    bool exited_sleep = false;
    for (int i = 0; i < 8; ++i) {
        robi::ImuSample picked_up = {0.0f, 0.92f, -0.35f, 25.0f, 0.0f, 0.0f};
        auto ev = trk.Step(picked_up, 40);
        if (ev.exit_face_down_sleep) exited_sleep = true;
    }
    assert(exited_sleep && !trk.is_face_down());

    // 4. Forehead game ("נחש מה על המצח"): works at ANY 360-degree roll of the round screen!
    trk.SetForeheadMode(true);
    // Wait out initial 300ms refractory + arm upright at rotated 60-degree grip (`ax=0.85, ay=0.50, az=0.0`)
    for (int i = 0; i < 16; ++i) {
        auto ev = trk.Step({0.85f, 0.50f, 0.02f, 5.0f, 5.0f, 5.0f}, 40);
        assert(ev.forehead_action == robi::ForeheadAction::kNone);
    }
    assert(trk.is_forehead_armed());

    // Nod glass toward floor (`screen_z = -0.82g` -> `az = +0.82g` since z_front_sign == -1) -> kCorrectDown!
    int correct_hits = 0;
    for (int i = 0; i < 6; ++i) {
        auto ev = trk.Step({0.45f, 0.25f, 0.82f, 60.0f, 0.0f, 0.0f}, 40);
        if (ev.forehead_action == robi::ForeheadAction::kCorrectDown) ++correct_hits;
        assert(ev.forehead_action != robi::ForeheadAction::kSkipUp);
    }
    assert(correct_hits == 1);

    // Immediate rebound overshoot upward within 650ms refractory must NOT false-trigger kSkipUp!
    for (int i = 0; i < 10; ++i) {
        auto ev = trk.Step({0.45f, 0.25f, -0.85f, -80.0f, 0.0f, 0.0f}, 40);
        assert(ev.forehead_action == robi::ForeheadAction::kNone);
    }

    // Re-arm neutral upright, then tilt glass toward ceiling (`az = -0.80g` -> `screen_z = +0.80g`) -> kSkipUp!
    for (int i = 0; i < 15; ++i) {
        trk.Step({-0.50f, 0.85f, -0.05f, 3.0f, 3.0f, 3.0f}, 40);
    }
    assert(trk.is_forehead_armed());
    int skip_hits = 0;
    for (int i = 0; i < 6; ++i) {
        auto ev = trk.Step({-0.30f, 0.45f, -0.80f, -55.0f, 0.0f, 0.0f}, 40);
        if (ev.forehead_action == robi::ForeheadAction::kSkipUp) ++skip_hits;
    }
    assert(skip_hits == 1);
}

void TestSfxSynthAndGamesEngine() {
    // 1. Verify all 8 procedural SFX IDs produce non-empty, click-free, non-clipping 24kHz PCM
    for (int id = 0; id <= 7; ++id) {
        auto pcm = robi::SynthesizeSfxPcm(static_cast<robi::RobiSfxId>(id), 24000);
        assert(pcm.size() >= 1200 && pcm.size() <= 24000);
        int max_abs = 0;
        for (int16_t s : pcm) {
            max_abs = std::max(max_abs, std::abs(static_cast<int>(s)));
        }
        assert(max_abs > 2000 && max_abs <= 28000);
        assert(pcm.back() == 0);  // Trailing zero settle sample
    }

    // 2. Verify every built-in Hebrew & English word across all 5 categories is strict valid UTF-8
    for (const auto& cat : robi::kHeadsUpCategories) {
        assert(IsStrictlyValidUtf8(cat.title_he));
        assert(cat.count_he >= 30 && cat.count_en >= 20);
        for (size_t i = 0; i < cat.count_he; ++i) {
            assert(std::strlen(cat.words_he[i]) > 0);
            assert(IsStrictlyValidUtf8(cat.words_he[i]));
        }
        for (size_t i = 0; i < cat.count_en; ++i) {
            assert(std::strlen(cat.words_en[i]) > 0);
            assert(IsStrictlyValidUtf8(cat.words_en[i]));
        }
    }

    // 3. Test HeadsUpEngine full lifecycle (3s countdown -> play -> flash -> summary)
    robi::HeadsUpEngine hu;
    hu.SeedRng(42);
    hu.StartOfflineRound(-1, false, 20);
    assert(hu.phase() == robi::HeadsUpEngine::Phase::kCountdown);
    assert(hu.countdown_sec() == 3);

    bool started = false;
    for (int i = 0; i < 32; ++i) {
        auto ev = hu.Step(100);
        if (ev.round_started) started = true;
    }
    assert(started && hu.phase() == robi::HeadsUpEngine::Phase::kPlaying);
    std::string w1 = hu.CurrentWord();
    assert(!w1.empty() && IsStrictlyValidUtf8(w1));

    assert(hu.MarkCorrect());
    assert(hu.phase() == robi::HeadsUpEngine::Phase::kFlashCorrect);
    assert(hu.correct_count() == 1);
    for (int i = 0; i < 10; ++i) hu.Step(100);
    assert(hu.phase() == robi::HeadsUpEngine::Phase::kPlaying);
    assert(hu.CurrentWord() != w1);

    // 4. Test deterministic 8-Question TriviaEngine (Solo + Head-to-Head + Shuffling + Fallback)
    for (const auto& tcat : robi::kTriviaCategories) {
        assert(IsStrictlyValidUtf8(tcat.title_he));
        assert(IsStrictlyValidUtf8(tcat.ai_topic_he));
        assert(tcat.count_he >= 8 && tcat.count_en >= 6);
        for (size_t i = 0; i < tcat.count_he; ++i) {
            assert(std::strlen(tcat.items_he[i].q) > 0 && IsStrictlyValidUtf8(tcat.items_he[i].q));
            assert(std::strlen(tcat.items_he[i].o0) > 0 && IsStrictlyValidUtf8(tcat.items_he[i].o0));
            assert(std::strlen(tcat.items_he[i].o1) > 0 && IsStrictlyValidUtf8(tcat.items_he[i].o1));
            assert(std::strlen(tcat.items_he[i].o2) > 0 && IsStrictlyValidUtf8(tcat.items_he[i].o2));
            assert(std::strlen(tcat.items_he[i].o3) > 0 && IsStrictlyValidUtf8(tcat.items_he[i].o3));
        }
        for (size_t i = 0; i < tcat.count_en; ++i) {
            assert(std::strlen(tcat.items_en[i].q) > 0 && IsStrictlyValidUtf8(tcat.items_en[i].q));
            assert(std::strlen(tcat.items_en[i].o0) > 0 && IsStrictlyValidUtf8(tcat.items_en[i].o0));
            assert(std::strlen(tcat.items_en[i].o1) > 0 && IsStrictlyValidUtf8(tcat.items_en[i].o1));
            assert(std::strlen(tcat.items_en[i].o2) > 0 && IsStrictlyValidUtf8(tcat.items_en[i].o2));
            assert(std::strlen(tcat.items_en[i].o3) > 0 && IsStrictlyValidUtf8(tcat.items_en[i].o3));
        }
    }

    robi::TriviaEngine tr;
    tr.SeedRng(20261118u);
    tr.StartOfflineDeck(-1, /*head_to_head=*/true, "לביא", "אורי", /*english=*/false, 8);
    assert(tr.IsActive() && tr.phase() == robi::TriviaEngine::Phase::kQuestion);
    assert(tr.total_questions() == 8);

    bool slot_seen[4] = {false, false, false, false};
    for (size_t qi = 0; qi < 8; ++qi) {
        assert(tr.question_index() == qi);
        assert(tr.CurrentPlayerIndex() == static_cast<int>(qi % 2));
        assert(tr.CurrentPlayerName() == ((qi % 2 == 0) ? "לביא" : "אורי"));
        const auto* q = tr.CurrentQuestion();
        assert(q != nullptr && q->options.size() == 4);
        assert(q->correct_index >= 0 && q->correct_index < 4);
        slot_seen[q->correct_index] = true;

        // Lavi (even turns: 0,2,4,6) answers 3/4 right; Ori (odd turns: 1,3,5,7) answers 4/4 right!
        int pick = (qi == 2) ? ((q->correct_index + 1) % 4) : q->correct_index;
        assert(tr.SubmitAnswer(pick));
        assert(tr.phase() == robi::TriviaEngine::Phase::kReveal);
        assert(!tr.SubmitAnswer(0));  // Double-tap during reveal must be ignored!

        bool next_q = false;
        bool summary = false;
        for (int step = 0; step < 16; ++step) {
            auto tev = tr.Step(100);
            if (tev.advanced_to_next_question) next_q = true;
            if (tev.finished_to_summary) summary = true;
        }
        if (qi < 7) {
            assert(next_q && !summary);
            assert(tr.phase() == robi::TriviaEngine::Phase::kQuestion);
        } else {
            assert(summary && !next_q);
            assert(tr.phase() == robi::TriviaEngine::Phase::kSummary);
        }
    }
    assert(tr.score(0) == 3 && tr.score(1) == 4);
    assert(tr.FormatHeaderText().find("אורי") != std::string::npos);
    assert(IsStrictlyValidUtf8(tr.FormatSummaryBody()));
    int distinct_slots = (slot_seen[0] ? 1 : 0) + (slot_seen[1] ? 1 : 0) +
                         (slot_seen[2] ? 1 : 0) + (slot_seen[3] ? 1 : 0);
    assert(distinct_slots >= 2);  // Options were genuinely shuffled across indices

    // Test preserve_first_question_order for voice-launched custom deck + loading timeout fallback
    std::vector<robi::TriviaQuestionItem> custom_qs = {
        {"שאלה ראשונה קבועה?", {"נכון1", "לא2", "לא3", "לא4"}, 0},
        {"שאלה שנייה?", {"נכון2", "לא2", "לא3", "לא4"}, 0},
        {"שאלה שלישית?", {"נכון3", "לא2", "לא3", "לא4"}, 0},
        {"שאלה רביעית?", {"נכון4", "לא2", "לא3", "לא4"}, 0},
    };
    tr.StartCustomDeck("כדורגל", custom_qs, 2, false, "לביא", "אורי", false, 4, true);
    assert(tr.CurrentQuestion() != nullptr);
    assert(tr.CurrentQuestion()->question == "שאלה ראשונה קבועה?");
    assert(tr.CurrentQuestion()->options[static_cast<size_t>(tr.CurrentQuestion()->correct_index)] == "נכון1");

    tr.BeginLoading("חלל", false, "לביא", "אורי", false);
    assert(tr.phase() == robi::TriviaEngine::Phase::kLoading);
    bool timed_out = false;
    for (int i = 0; i < 60; ++i) {
        if (tr.Step(100).loading_timed_out) timed_out = true;
    }
    assert(timed_out);

    // 5. Test RobiGameHub request-id cancellation & mic-mute gate
    auto& hub = robi::RobiGameHub::GetInstance();
    uint32_t r1 = hub.NextTriviaRequestId();
    uint32_t r2 = hub.NextTriviaRequestId();
    hub.DeliverAsyncTriviaDeck(r1, custom_qs);  // Stale r1 must be ignored!
    std::vector<robi::TriviaQuestionItem> popped_qs;
    assert(!hub.PopAsyncTriviaDeck(r2, popped_qs));
    hub.DeliverAsyncTriviaDeck(r2, custom_qs);
    assert(hub.PopAsyncTriviaDeck(r2, popped_qs) && popped_qs.size() == 4);
    assert(!hub.PopAsyncTriviaDeck(r2, popped_qs));

    hub.SetMicMutedForScreenGame(true);
    assert(hub.IsMicMutedForScreenGame());
    hub.SetMicMutedForScreenGame(false);
    assert(!hub.IsMicMutedForScreenGame());

    auto dizzy_res = robi::RobiComputePresetParams("dizzy", robi::ROBI_THEME_CYBER_CYAN,
                                                   robi::ROBI_STYLE_CYBER_VISOR);
    assert(dizzy_res.tremor_shock);
    assert(std::strcmp(robi::RobiNormalizeEmotionName("wink"), "winking") == 0);
    assert(std::strcmp(robi::RobiNormalizeEmotionName("love"), "loving") == 0);
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
    TestImuGesturesAndForeheadTilt();
    TestSfxSynthAndGamesEngine();
    std::printf("ALL ROBI v3.2 SYNC, IMU, SFX & GAMES UNIT TESTS PASSED!\n");
    return 0;
}
