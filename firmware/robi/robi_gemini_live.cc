#include "robi_gemini_live.h"
#include "robi_config.h"

#include "application.h"
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "display.h"

#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_tls.h>
#include <mbedtls/base64.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#define TAG "GeminiLive"

extern "C" {

int __real_esp_tls_conn_new_sync(const char* hostname, int hostlen, int port,
                                 const esp_tls_cfg_t* cfg, esp_tls_t* tls);

int __wrap_esp_tls_conn_new_sync(const char* hostname, int hostlen, int port,
                                 const esp_tls_cfg_t* cfg, esp_tls_t* tls) {
    if (cfg != nullptr) {
        esp_tls_cfg_t patched = *cfg;
        if (patched.timeout_ms <= 0) {
            patched.timeout_ms = 8000;
        }
        return __real_esp_tls_conn_new_sync(hostname, hostlen, port, &patched, tls);
    }
    return __real_esp_tls_conn_new_sync(hostname, hostlen, port, cfg, tls);
}

ssize_t __real_esp_tls_conn_read(esp_tls_t* tls, void* data, size_t datalen);

ssize_t __wrap_esp_tls_conn_read(esp_tls_t* tls, void* data, size_t datalen) {
    if (uxTaskPriorityGet(nullptr) < 5) {
        vTaskPrioritySet(nullptr, 5);
    }
    return __real_esp_tls_conn_read(tls, data, datalen);
}

}  // extern "C"

namespace {

constexpr EventBits_t kEvtSetupComplete = (1 << 0);
constexpr EventBits_t kEvtWsClosed = (1 << 1);

constexpr EventBits_t kDoneRx = (1 << 0);
constexpr EventBits_t kDoneTx = (1 << 1);
constexpr EventBits_t kDonePlay = (1 << 2);
constexpr EventBits_t kDoneAll = kDoneRx | kDoneTx | kDonePlay;

constexpr size_t kPsramRingSamples = 262144;   // 512 KB = 10.92s @ 24kHz
constexpr size_t kFallbackRingSamples = 48000;
constexpr size_t kPreBufferSamples = 7200;     // 300ms @ 24kHz cushion before starting speaker
constexpr size_t kRebufferSamples = 3600;      // 150ms @ 24kHz cushion if network stalls mid-turn
constexpr size_t kPlaybackFrameSamples = 1200; // 50ms @ 24kHz (divides 2400/4800/9600 evenly!)

int64_t g_model_cooldown_until_us[3] = {0, 0, 0};
std::mutex g_last_model_mutex;
std::string g_last_connected_model;
GeminiLiveProtocol* g_active_protocol = nullptr;

struct EmojiMapping {
    const char* utf8;
    const char* emotion;
};

constexpr EmojiMapping kEmojiMap[] = {
    {"😂", "laughing"}, {"🤣", "laughing"}, {"😆", "laughing"},
    {"🤩", "surprised"}, {"😮", "surprised"}, {"😯", "surprised"}, {"🌟", "surprised"},
    {"😱", "shocked"},  {"🤯", "shocked"},  {"⚡", "shocked"},
    {"🥰", "loving"},   {"😍", "loving"},   {"❤", "loving"},    {"💖", "loving"}, {"🤗", "loving"},
    {"😘", "kissy"},    {"😗", "kissy"},
    {"😉", "winking"},  {"😜", "winking"},  {"😝", "winking"},
    {"😎", "cool"},     {"🤖", "cool"},     {"🚀", "cool"},
    {"🤔", "thinking"}, {"🧐", "thinking"}, {"💭", "thinking"},
    {"😕", "confused"}, {"🤨", "confused"},
    {"🤪", "silly"},    {"😛", "silly"},
    {"😳", "embarrassed"}, {"🙈", "embarrassed"},
    {"😌", "relaxed"},  {"😋", "relaxed"},
    {"😢", "sad"},      {"😔", "sad"},
    {"😭", "crying"},
    {"😠", "angry"},    {"😡", "angry"},
    {"😴", "sleepy"},   {"🥱", "sleepy"},
    {"😏", "confident"},{"💪", "confident"},
    {"😄", "happy"},    {"😊", "happy"},    {"🙂", "happy"},    {"😁", "happy"}, {"🎉", "happy"},
};

const char* MatchLeadingOrInlineEmoji(const std::string& s) {
    for (const auto& m : kEmojiMap) {
        if (s.find(m.utf8) != std::string::npos) {
            return m.emotion;
        }
    }
    return nullptr;
}

const char* InferHebrewClauseEmotion(const std::string& clause) {
    if (clause.size() < 12) return nullptr;
    auto has = [&](const char* kw) { return clause.find(kw) != std::string::npos; };

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
    if (!clause.empty() && clause.back() == '?') return "thinking";
    return nullptr;
}

bool IsWakeWordToken(const std::string& text) {
    std::string low;
    low.reserve(text.size());
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') {
            low.push_back(static_cast<char>(c + 32));
        } else {
            low.push_back(static_cast<char>(c));
        }
    }
    return low == "hey robby" || low == "hey robi" || low == "hi,esp" ||
           low == "hi esp" || low == "hiesp";
}

std::string Base64Encode(const uint8_t* src, size_t len) {
    if (len == 0) return {};
    size_t olen = 0;
    mbedtls_base64_encode(nullptr, 0, &olen, src, len);
    std::string out(olen, '\0');
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(out.data()), out.size(), &olen,
                              src, len) != 0) {
        return {};
    }
    out.resize(olen);
    return out;
}

}  // namespace

GeminiLiveProtocol::GeminiLiveProtocol() {
    g_active_protocol = this;
    server_sample_rate_ = 24000;
    server_frame_duration_ = 60;
    event_group_ = xEventGroupCreate();
    worker_done_group_ = xEventGroupCreate();

    pcm_ring_ = static_cast<int16_t*>(
        heap_caps_malloc(kPsramRingSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcm_ring_ != nullptr) {
        ring_capacity_ = kPsramRingSamples;
    } else {
        pcm_ring_ = static_cast<int16_t*>(malloc(kFallbackRingSamples * sizeof(int16_t)));
        ring_capacity_ = pcm_ring_ ? kFallbackRingSamples : 0;
    }

    rx_stack_ = static_cast<StackType_t*>(heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    rx_tcb_ = static_cast<StaticTask_t*>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    tx_stack_ = static_cast<StackType_t*>(heap_caps_malloc(10240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    tx_tcb_ = static_cast<StaticTask_t*>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    play_stack_ = static_cast<StackType_t*>(heap_caps_malloc(10240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    play_tcb_ = static_cast<StaticTask_t*>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

GeminiLiveProtocol::~GeminiLiveProtocol() {
    if (g_active_protocol == this) {
        g_active_protocol = nullptr;
    }
    StopSessionInternal(false);
    if (rx_stack_) heap_caps_free(rx_stack_);
    if (rx_tcb_) heap_caps_free(rx_tcb_);
    if (tx_stack_) heap_caps_free(tx_stack_);
    if (tx_tcb_) heap_caps_free(tx_tcb_);
    if (play_stack_) heap_caps_free(play_stack_);
    if (play_tcb_) heap_caps_free(play_tcb_);
    if (pcm_ring_ != nullptr) {
        heap_caps_free(pcm_ring_);
        pcm_ring_ = nullptr;
    }
    if (worker_done_group_ != nullptr) {
        vEventGroupDelete(worker_done_group_);
    }
    if (event_group_ != nullptr) {
        vEventGroupDelete(event_group_);
    }
}

GeminiLiveProtocol* GeminiLiveProtocol::GetActiveInstance() {
    return g_active_protocol;
}

std::string GeminiLiveProtocol::GetActiveModel() const {
    std::lock_guard<std::mutex> lk(model_mutex_);
    return active_model_;
}

std::string GeminiLiveProtocol::GetLastConnectedModel() {
    std::lock_guard<std::mutex> lk(g_last_model_mutex);
    return g_last_connected_model;
}

int GeminiLiveProtocol::GetModelCooldownSec(int idx) {
    if (idx < 0 || idx >= 3) return 0;
    int64_t now_us = esp_timer_get_time();
    int64_t until_us = g_model_cooldown_until_us[idx];
    if (until_us <= now_us) return 0;
    return static_cast<int>((until_us - now_us + 999999LL) / 1000000LL);
}

void GeminiLiveProtocol::ResetModelCooldowns() {
    for (int i = 0; i < 3; ++i) {
        g_model_cooldown_until_us[i] = 0;
    }
}

bool GeminiLiveProtocol::Start() {
    return true;
}

bool GeminiLiveProtocol::IsAudioChannelOpened() const {
    return channel_opened_.load() && ws_connected_.load() && !error_occurred_;
}

void GeminiLiveProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;
    bool was_open = channel_opened_.load();
    StopSessionInternal(was_open);
}

void GeminiLiveProtocol::StopSessionInternal(bool notify_closed) {
    stopping_.store(true);
    channel_opened_.store(false);
    ws_connected_.store(false);
    turn_gen_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(model_mutex_);
        active_model_.clear();
    }

    {
        std::lock_guard<std::mutex> lk(rx_mutex_);
        rx_queue_.clear();
    }
    rx_cv_.notify_all();

    {
        std::lock_guard<std::mutex> lk(tx_mutex_);
        tx_queue_.clear();
    }
    tx_cv_.notify_all();

    ClearRing();
    ring_cv_.notify_all();

    if (rx_task_ != nullptr || tx_task_ != nullptr || play_task_ != nullptr) {
        xEventGroupWaitBits(worker_done_group_, kDoneAll, pdFALSE, pdTRUE, pdMS_TO_TICKS(4000));
        rx_task_ = nullptr;
        tx_task_ = nullptr;
        play_task_ = nullptr;
    }

    if (websocket_ != nullptr) {
        websocket_.reset();
    }

    if (notify_closed && on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

std::string GeminiLiveProtocol::BuildSetupJson(const std::string& model_name,
                                               const std::string& voice_name,
                                               const std::string& lang_code,
                                               bool include_lang_code) const {
    std::string sys_prompt = RobiConfig::GetInstance().BuildSystemPrompt();

    cJSON* root = cJSON_CreateObject();
    cJSON* setup = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "setup", setup);

    std::string full_model = "models/" + model_name;
    cJSON_AddStringToObject(setup, "model", full_model.c_str());

    cJSON* gen_cfg = cJSON_CreateObject();
    cJSON* modalities = cJSON_CreateArray();
    cJSON_AddItemToArray(modalities, cJSON_CreateString("AUDIO"));
    cJSON_AddItemToObject(gen_cfg, "responseModalities", modalities);

    cJSON* speech_cfg = cJSON_CreateObject();
    if (include_lang_code && !lang_code.empty()) {
        cJSON_AddStringToObject(speech_cfg, "languageCode", lang_code.c_str());
    }
    cJSON* voice_cfg = cJSON_CreateObject();
    cJSON* prebuilt = cJSON_CreateObject();
    cJSON_AddStringToObject(prebuilt, "voiceName", voice_name.c_str());
    cJSON_AddItemToObject(voice_cfg, "prebuiltVoiceConfig", prebuilt);
    cJSON_AddItemToObject(speech_cfg, "voiceConfig", voice_cfg);
    cJSON_AddItemToObject(gen_cfg, "speechConfig", speech_cfg);
    cJSON_AddItemToObject(setup, "generationConfig", gen_cfg);

    cJSON* rt_cfg = cJSON_CreateObject();
    cJSON* aad = cJSON_CreateObject();
    cJSON_AddNumberToObject(aad, "silenceDurationMs", 500);
    cJSON_AddItemToObject(rt_cfg, "automaticActivityDetection", aad);
    cJSON_AddItemToObject(setup, "realtimeInputConfig", rt_cfg);

    cJSON* sys_inst = cJSON_CreateObject();
    cJSON* sys_parts = cJSON_CreateArray();
    cJSON* sys_part0 = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_part0, "text", sys_prompt.c_str());
    cJSON_AddItemToArray(sys_parts, sys_part0);
    cJSON_AddItemToObject(sys_inst, "parts", sys_parts);
    cJSON_AddItemToObject(setup, "systemInstruction", sys_inst);

    // Tools declarations
    cJSON* tools_arr = cJSON_CreateArray();
    cJSON* tools_obj = cJSON_CreateObject();
    cJSON* fdecls = cJSON_CreateArray();

    auto add_empty_tool = [&](const char* name, const char* desc) {
        cJSON* fn = cJSON_CreateObject();
        cJSON_AddStringToObject(fn, "name", name);
        cJSON_AddStringToObject(fn, "description", desc);
        cJSON* params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "type", "OBJECT");
        cJSON_AddItemToObject(params, "properties", cJSON_CreateObject());
        cJSON_AddItemToObject(fn, "parameters", params);
        cJSON_AddItemToArray(fdecls, fn);
    };

    auto add_int_tool = [&](const char* name, const char* desc, const char* arg_name,
                            const char* arg_desc) {
        cJSON* fn = cJSON_CreateObject();
        cJSON_AddStringToObject(fn, "name", name);
        cJSON_AddStringToObject(fn, "description", desc);
        cJSON* params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "type", "OBJECT");
        cJSON* props = cJSON_CreateObject();
        cJSON* prop = cJSON_CreateObject();
        cJSON_AddStringToObject(prop, "type", "INTEGER");
        cJSON_AddStringToObject(prop, "description", arg_desc);
        cJSON_AddItemToObject(props, arg_name, prop);
        cJSON_AddItemToObject(params, "properties", props);
        cJSON* req = cJSON_CreateArray();
        cJSON_AddItemToArray(req, cJSON_CreateString(arg_name));
        cJSON_AddItemToObject(params, "required", req);
        cJSON_AddItemToObject(fn, "parameters", params);
        cJSON_AddItemToArray(fdecls, fn);
    };

    add_empty_tool("handle_exit_intent",
                   "Call AFTER saying a warm goodbye when the child wants to stop, sleep, or leave.");
    add_empty_tool("get_device_status",
                   "Get battery percentage, charging status, speaker volume, brightness, and WiFi info.");
    add_int_tool("set_volume", "Set speaker volume from 0 to 100.", "volume", "Volume 0-100");
    add_int_tool("set_brightness", "Set screen brightness from 10 to 100.", "brightness",
                 "Brightness 10-100");

    // save_memory(note: string)
    {
        cJSON* fn = cJSON_CreateObject();
        cJSON_AddStringToObject(fn, "name", "save_memory");
        cJSON_AddStringToObject(
            fn, "description",
            "Save a short note (up to 200 chars) in Hebrew or English whenever the child asks an interesting question, reveals a personal detail/friend/hobby/preference, solves a challenge, or progresses in a story so you learn about the child and remember next time.");
        cJSON* params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "type", "OBJECT");
        cJSON* props = cJSON_CreateObject();
        cJSON* prop = cJSON_CreateObject();
        cJSON_AddStringToObject(prop, "type", "STRING");
        cJSON_AddStringToObject(prop, "description", "Short memory summary note");
        cJSON_AddItemToObject(props, "note", prop);
        cJSON_AddItemToObject(params, "properties", props);
        cJSON* req = cJSON_CreateArray();
        cJSON_AddItemToArray(req, cJSON_CreateString("note"));
        cJSON_AddItemToObject(params, "required", req);
        cJSON_AddItemToObject(fn, "parameters", params);
        cJSON_AddItemToArray(fdecls, fn);
    }

    cJSON_AddItemToObject(tools_obj, "functionDeclarations", fdecls);
    cJSON_AddItemToArray(tools_arr, tools_obj);
    cJSON_AddItemToObject(setup, "tools", tools_arr);

    cJSON_AddItemToObject(setup, "inputAudioTranscription", cJSON_CreateObject());
    cJSON_AddItemToObject(setup, "outputAudioTranscription", cJSON_CreateObject());

    char* printed = cJSON_PrintUnformatted(root);
    std::string out = printed ? printed : "{}";
    if (printed) cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

bool GeminiLiveProtocol::TryConnectModel(const std::string& api_key, const std::string& model_name,
                                         const std::string& voice_name,
                                         const std::string& lang_code, bool include_lang_code) {
    StopSessionInternal(false);

    stopping_.store(false);
    error_occurred_ = false;
    drop_turn_.store(false);
    turn_complete_.store(false);
    close_after_turn_.store(false);
    awaiting_response_.store(false);
    speaking_active_.store(false);
    awaiting_since_us_.store(0);
    tool_hold_until_us_.store(0);
    last_rx_audio_us_.store(esp_timer_get_time());
    awaiting_nudges_ = 0;
    idle_farewell_sent_ = false;
    last_activity_us_.store(esp_timer_get_time());
    {
        std::lock_guard<std::mutex> lk(transcript_mutex_);
        pending_user_transcript_.clear();
        pending_bot_transcript_.clear();
    }

    xEventGroupClearBits(event_group_, kEvtSetupComplete | kEvtWsClosed);
    xEventGroupClearBits(worker_done_group_, kDoneAll);

    auto network = Board::GetInstance().GetNetwork();
    websocket_ = network->CreateWebSocket(1);
    if (websocket_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create WebSocket instance");
        return false;
    }

    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        (void)binary;
        if (stopping_.load() || len == 0 || data == nullptr) return;
        {
            std::lock_guard<std::mutex> lk(rx_mutex_);
            if (rx_queue_.size() < 64) {
                rx_queue_.emplace_back(data, len);
            }
        }
        rx_cv_.notify_one();
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGW(TAG, "Gemini Live WebSocket disconnected");
        ws_connected_.store(false);
        xEventGroupSetBits(event_group_, kEvtWsClosed);
        rx_cv_.notify_all();
        tx_cv_.notify_all();
        ring_cv_.notify_all();
        if (channel_opened_.exchange(false) && !stopping_.load()) {
            if (on_audio_channel_closed_ != nullptr) {
                on_audio_channel_closed_();
            }
        }
    });

    std::string url =
        "wss://generativelanguage.googleapis.com/ws/"
        "google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=" +
        api_key;

    ESP_LOGI(TAG, "Connecting to Gemini Live (%s / %s)...", model_name.c_str(), voice_name.c_str());
    bool ws_ok = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto connected = websocket_->Connect(url.c_str());
        if (connected) {
            ws_ok = true;
            break;
        }
        ESP_LOGW(TAG, "Connect attempt %d failed (%s): %s", attempt + 1, model_name.c_str(),
                 connected.error().ToString().c_str());
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    if (!ws_ok) {
        websocket_.reset();
        return false;
    }

    ws_connected_.store(true);

    // Launch worker tasks using pre-allocated PSRAM stacks so Internal SRAM is never exhausted
    rx_task_ = xTaskCreateStatic(
        [](void* arg) {
            auto* self = static_cast<GeminiLiveProtocol*>(arg);
            self->RxWorkerLoop();
            xEventGroupSetBits(self->worker_done_group_, kDoneRx);
            vTaskDelete(nullptr);
        },
        "gemini_rx", 16384, this, 3, rx_stack_, rx_tcb_);

    tx_task_ = xTaskCreateStatic(
        [](void* arg) {
            auto* self = static_cast<GeminiLiveProtocol*>(arg);
            self->TxWorkerLoop();
            xEventGroupSetBits(self->worker_done_group_, kDoneTx);
            vTaskDelete(nullptr);
        },
        "gemini_tx", 10240, this, 3, tx_stack_, tx_tcb_);

    play_task_ = xTaskCreateStatic(
        [](void* arg) {
            auto* self = static_cast<GeminiLiveProtocol*>(arg);
            self->PlaybackFeederLoop();
            xEventGroupSetBits(self->worker_done_group_, kDonePlay);
            vTaskDelete(nullptr);
        },
        "gemini_play", 10240, this, 4, play_stack_, play_tcb_);

    std::string setup_msg = BuildSetupJson(model_name, voice_name, lang_code, include_lang_code);
    if (!websocket_->Send(setup_msg)) {
        ESP_LOGW(TAG, "Failed to send Gemini setup message (%s)", model_name.c_str());
        StopSessionInternal(false);
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(event_group_, kEvtSetupComplete | kEvtWsClosed, pdTRUE,
                                           pdFALSE, pdMS_TO_TICKS(8000));
    if (bits & kEvtSetupComplete) {
        ESP_LOGI(TAG, "Gemini Live ready! (%s / %s)", model_name.c_str(), voice_name.c_str());
        {
            std::lock_guard<std::mutex> lk(model_mutex_);
            active_model_ = model_name;
        }
        {
            std::lock_guard<std::mutex> lk(g_last_model_mutex);
            g_last_connected_model = model_name;
        }
        turn_uplink_pcm_bytes_.store(0);
        turn_downlink_samples_.store(0);
        turn_server_tokens_.store(0);
        RobiConfig::GetInstance().RecordModelSession(model_name);
        channel_opened_.store(true);
        if (on_connected_) on_connected_();
        if (on_audio_channel_opened_) on_audio_channel_opened_();
        return true;
    }

    ESP_LOGW(TAG, "Gemini setup did not complete for %s (bits=0x%x)", model_name.c_str(),
             static_cast<unsigned>(bits));
    StopSessionInternal(false);
    return false;
}

bool GeminiLiveProtocol::OpenAudioChannel() {
    auto cfg = RobiConfig::GetInstance().Get();
    if (cfg.gemini_key.empty()) {
        std::string ip = WifiManager::GetInstance().GetIpAddress();
        std::string msg = "הגדר מפתח ג'מיני בכתובת http://robi.local";
        if (!ip.empty()) {
            msg += " או http://" + ip;
        }
        SetError(msg);
        return false;
    }

    struct Candidate {
        const char* model;
        bool with_lang;
        int idx;
    };
    const Candidate base_candidates[3] = {
        {"gemini-3.8-live", true, 0},
        {"gemini-2.5-flash-native-audio-preview-12-2025", false, 1},
        {"gemini-2.0-flash-live-001", true, 2},
    };

    Candidate ordered[3] = {base_candidates[0], base_candidates[1], base_candidates[2]};
    if (!cfg.preferred_model.empty() && cfg.preferred_model != "auto") {
        int pref_pos = -1;
        for (int i = 0; i < 3; ++i) {
            if (cfg.preferred_model == base_candidates[i].model) {
                pref_pos = i;
                break;
            }
        }
        if (pref_pos > 0) {
            ordered[0] = base_candidates[pref_pos];
            int out_i = 1;
            for (int i = 0; i < 3; ++i) {
                if (i != pref_pos) {
                    ordered[out_i++] = base_candidates[i];
                }
            }
        }
    }

    int64_t now_us = esp_timer_get_time();
    for (const auto& c : ordered) {
        if (now_us < g_model_cooldown_until_us[c.idx]) {
            continue;
        }
        if (TryConnectModel(cfg.gemini_key, c.model, cfg.voice, cfg.language, c.with_lang)) {
            g_model_cooldown_until_us[c.idx] = 0;
            return true;
        }
        // Cooldown failed model (10 min) so consecutive sessions connect immediately to the working fallback
        g_model_cooldown_until_us[c.idx] = esp_timer_get_time() + 600LL * 1000000LL;
    }

    // Retry cooled-down candidates once in case all were cooled down
    for (const auto& c : ordered) {
        if (now_us >= g_model_cooldown_until_us[c.idx]) continue;
        g_model_cooldown_until_us[c.idx] = 0;
        if (TryConnectModel(cfg.gemini_key, c.model, cfg.voice, cfg.language, c.with_lang)) {
            return true;
        }
        g_model_cooldown_until_us[c.idx] = esp_timer_get_time() + 600LL * 1000000LL;
    }

    SetError(Lang::Strings::SERVER_NOT_CONNECTED);
    return false;
}

bool GeminiLiveProtocol::SendText(const std::string& text) {
    if (!ws_connected_.load()) return false;
    EnqueueTxJson(std::string(text));
    return true;
}

void GeminiLiveProtocol::EnqueueTxJson(std::string&& json_str) {
    if (stopping_.load() || !ws_connected_.load()) return;
    {
        std::lock_guard<std::mutex> lk(tx_mutex_);
        if (tx_queue_.size() >= 120) {
            tx_queue_.pop_front();
        }
        tx_queue_.push_back(std::move(json_str));
    }
    tx_cv_.notify_one();
}

void GeminiLiveProtocol::EnqueuePcm16k(const uint8_t* pcm_bytes, size_t byte_len) {
    if (pcm_bytes == nullptr || byte_len == 0 || stopping_.load() || !ws_connected_.load()) return;
    constexpr size_t kChunkBytes = 3840;  // 120ms @ 16kHz int16
    for (size_t off = 0; off < byte_len; off += kChunkBytes) {
        size_t n = std::min(kChunkBytes, byte_len - off);
        std::string b64 = Base64Encode(pcm_bytes + off, n);
        if (b64.empty()) continue;
        std::string msg;
        msg.reserve(b64.size() + 80);
        msg += "{\"realtimeInput\":{\"audio\":{\"mimeType\":\"audio/pcm;rate=16000\",\"data\":\"";
        msg += b64;
        msg += "\"}}}";
        EnqueueTxJson(std::move(msg));
    }
}

void GeminiLiveProtocol::EnqueueSilence16k(size_t duration_ms) {
    size_t total_bytes = (duration_ms * 16000 / 1000) * sizeof(int16_t);
    std::vector<uint8_t> zeros(std::min<size_t>(total_bytes, 3840), 0);
    size_t rem = total_bytes;
    while (rem > 0) {
        size_t chunk = std::min(rem, zeros.size());
        EnqueuePcm16k(zeros.data(), chunk);
        rem -= chunk;
    }
}

void GeminiLiveProtocol::FinishSpeechUtterance() {
    streaming_speech_ = false;
    onset_frames_ = 0;
    speech_frames_ = 0;
    silence_frames_ = 0;
    if (!tx_pcm_batch_.empty()) {
        EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
        tx_pcm_batch_.clear();
    }
    drop_turn_.store(false);
    awaiting_response_.store(true);
    awaiting_since_us_.store(esp_timer_get_time());
    awaiting_nudges_ = 0;
    EnqueueSilence16k(1100);
    EmitEmotion("thinking");
}

bool GeminiLiveProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!IsAudioChannelOpened() || !packet || packet->payload.empty()) {
        return false;
    }
    if (Application::GetInstance().GetDeviceState() != kDeviceStateListening) {
        return true;
    }

    int64_t now_us = esp_timer_get_time();
    // Do not record or stream mic audio while waiting for Gemini's response or playing/holding a tool turn
    if (awaiting_response_.load() || speaking_active_.load() || RingAvailable() > 0 ||
        now_us < tool_hold_until_us_.load()) {
        return true;
    }

    std::lock_guard<std::mutex> lk(uplink_mutex_);

    // Ignore mic input for 160ms right after entering Listening so OGG_POPUP doesn't echo
    if (listen_started_us_ > 0 && (now_us - listen_started_us_) < 160000) {
        return true;
    }

    const auto* samples = reinterpret_cast<const int16_t*>(packet->payload.data());
    size_t sample_count = packet->payload.size() / sizeof(int16_t);
    if (sample_count == 0) return true;

    if (listening_mode_ == kListeningModeManualStop) {
        if (!streaming_speech_) {
            streaming_speech_ = true;
            drop_turn_.store(false);
            EnqueueSilence16k(180);
            EmitEmotion("listening");
        }
        last_activity_us_.store(now_us);
        tx_pcm_batch_.insert(tx_pcm_batch_.end(), packet->payload.begin(), packet->payload.end());
        if (tx_pcm_batch_.size() >= 3840) {
            EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
            tx_pcm_batch_.clear();
        }
        return true;
    }

    // Auto-stop turn-based mode: compute DC-removed AC RMS so ES7210 DC bias (~280 LSB)
    // and WebRTC VAD_MODE_0 ambient noise never false-trigger speech in silence!
    double mean = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        mean += static_cast<double>(samples[i]);
    }
    mean /= static_cast<double>(sample_count);

    double sum_sq = 0.0;
    for (size_t i = 0; i < sample_count; ++i) {
        double diff = static_cast<double>(samples[i]) - mean;
        sum_sq += diff * diff;
    }
    double ac_rms = std::sqrt(sum_sq / static_cast<double>(sample_count));

    if (!streaming_speech_) {
        bool voice_frame = (ac_rms >= 350.0);
        if (voice_frame) {
            ++onset_frames_;
        } else {
            onset_frames_ = 0;
        }
        if (onset_frames_ < 3) {
            if (pre_roll_.size() >= 12) {
                pre_roll_.pop_front();
            }
            pre_roll_.push_back(std::move(packet->payload));
            return true;
        }
        // Speech onset!
        streaming_speech_ = true;
        onset_frames_ = 0;
        speech_frames_ = 0;
        silence_frames_ = 0;
        idle_farewell_sent_ = false;
        drop_turn_.store(false);
        last_activity_us_.store(now_us);
        EmitEmotion("listening");

        EnqueueSilence16k(180);
        tx_pcm_batch_.clear();
        for (const auto& old_frame : pre_roll_) {
            tx_pcm_batch_.insert(tx_pcm_batch_.end(), old_frame.begin(), old_frame.end());
            if (tx_pcm_batch_.size() >= 3840) {
                EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
                tx_pcm_batch_.clear();
            }
        }
        pre_roll_.clear();
    }

    last_activity_us_.store(now_us);
    ++speech_frames_;
    turn_uplink_pcm_bytes_.fetch_add(static_cast<uint32_t>(packet->payload.size()));
    tx_pcm_batch_.insert(tx_pcm_batch_.end(), packet->payload.begin(), packet->payload.end());
    if (tx_pcm_batch_.size() >= 3840) {
        EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
        tx_pcm_batch_.clear();
    }

    if (ac_rms < 200.0) {
        ++silence_frames_;
        // 25 frames @ 32ms = 800ms of post-speech silence so natural word pauses don't cut off speech
        if (silence_frames_ >= 25) {
            FinishSpeechUtterance();
        }
    } else {
        silence_frames_ = 0;
        // Safety cap: commit utterance after ~11.2s (350 frames) even in noisy rooms
        if (speech_frames_ >= 350) {
            FinishSpeechUtterance();
        }
    }

    return true;
}

void GeminiLiveProtocol::EnqueueClientTextTurn(const std::string& text) {
    if (text.empty()) return;
    ESP_LOGI(TAG, "Client text turn: %s", text.c_str());
    drop_turn_.store(false);
    turn_complete_.store(false);
    awaiting_response_.store(true);
    awaiting_since_us_.store(esp_timer_get_time());
    awaiting_nudges_ = 0;
    last_activity_us_.store(esp_timer_get_time());
    RobiConfig::GetInstance().AddDialogueTurn("user", text);
    EmitEmotion("thinking");

    cJSON* root = cJSON_CreateObject();
    cJSON* cc = cJSON_CreateObject();
    cJSON* turns = cJSON_CreateArray();
    cJSON* turn0 = cJSON_CreateObject();
    cJSON_AddStringToObject(turn0, "role", "user");
    cJSON* parts = cJSON_CreateArray();
    cJSON* p0 = cJSON_CreateObject();
    cJSON_AddStringToObject(p0, "text", text.c_str());
    cJSON_AddItemToArray(parts, p0);
    cJSON_AddItemToObject(turn0, "parts", parts);
    cJSON_AddItemToArray(turns, turn0);
    cJSON_AddItemToObject(cc, "turns", turns);
    cJSON_AddBoolToObject(cc, "turnComplete", true);
    cJSON_AddItemToObject(root, "clientContent", cc);

    char* s = cJSON_PrintUnformatted(root);
    if (s != nullptr) {
        EnqueueTxJson(std::string(s));
        cJSON_free(s);
    }
    cJSON_Delete(root);
}

void GeminiLiveProtocol::SendWakeWordDetected(const std::string& wake_word) {
    if (wake_word.empty() || IsWakeWordToken(wake_word)) {
        return;  // Standard wake words go straight to OGG_POPUP + Listening
    }
    EnqueueClientTextTurn(wake_word);
}

bool GeminiLiveProtocol::InjectUserPromptTurn(const std::string& text) {
    if (text.empty()) return false;
    Application::GetInstance().Schedule([this, text]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateIdle || !IsAudioChannelOpened()) {
            app.WakeWordInvoke(text);
        } else {
            if (app.GetDeviceState() == kDeviceStateSpeaking) {
                app.AbortSpeaking(kAbortReasonNone);
            }
            EnqueueClientTextTurn(text);
        }
    });
    return true;
}

bool GeminiLiveProtocol::SimulateMicUtterance(int duration_ms) {
    if (!IsAudioChannelOpened()) return false;
    int clamped_ms = std::clamp(duration_ms, 300, 3000);
    xTaskCreate(
        [](void* arg) {
            int dur_ms = static_cast<int>(reinterpret_cast<intptr_t>(arg));
            auto* proto = GeminiLiveProtocol::GetActiveInstance();
            if (proto == nullptr || !proto->IsAudioChannelOpened()) {
                vTaskDelete(nullptr);
                return;
            }
            // Wait slightly past the 160ms OGG_POPUP guard
            vTaskDelay(pdMS_TO_TICKS(200));
            constexpr size_t kFrameSamples = 512;  // 32ms @ 16kHz
            int active_frames = std::max(10, dur_ms / 32);
            double phase = 0.0;
            for (int f = 0; f < active_frames; ++f) {
                auto pkt = std::make_unique<AudioStreamPacket>();
                pkt->sample_rate = 16000;
                pkt->frame_duration = 32;
                pkt->payload.resize(kFrameSamples * sizeof(int16_t));
                auto* s = reinterpret_cast<int16_t*>(pkt->payload.data());
                for (size_t i = 0; i < kFrameSamples; ++i) {
                    s[i] = static_cast<int16_t>(1800.0 * std::sin(phase) + 900.0 * std::sin(phase * 2.1));
                    phase += 2.0 * 3.141592653589793 * 320.0 / 16000.0;
                }
                proto->SendAudio(std::move(pkt));
                vTaskDelay(pdMS_TO_TICKS(28));
            }
            // Follow with 28 frames (~900ms) of silence to trigger FinishSpeechUtterance()
            for (int f = 0; f < 28; ++f) {
                auto pkt = std::make_unique<AudioStreamPacket>();
                pkt->sample_rate = 16000;
                pkt->frame_duration = 32;
                pkt->payload.assign(kFrameSamples * sizeof(int16_t), 0);
                proto->SendAudio(std::move(pkt));
                vTaskDelay(pdMS_TO_TICKS(28));
            }
            vTaskDelete(nullptr);
        },
        "robi_sim_mic", 4096, reinterpret_cast<void*>(static_cast<intptr_t>(clamped_ms)), 3, nullptr);
    return true;
}

void GeminiLiveProtocol::SendStartListening(ListeningMode mode) {
    std::lock_guard<std::mutex> lk(uplink_mutex_);
    listening_mode_ = mode;
    pre_roll_.clear();
    tx_pcm_batch_.clear();
    streaming_speech_ = false;
    onset_frames_ = 0;
    speech_frames_ = 0;
    silence_frames_ = 0;
    int64_t now_us = esp_timer_get_time();
    // Do NOT clear awaiting_response_ if a prompt/wake-word turn was just submitted (< 8s ago)!
    if (!awaiting_response_.load() || (now_us - awaiting_since_us_.load()) > 8000000LL) {
        awaiting_response_.store(false);
    }
    listen_started_us_ = now_us;
    last_activity_us_.store(now_us);
}

void GeminiLiveProtocol::SendStopListening() {
    std::lock_guard<std::mutex> lk(uplink_mutex_);
    if (streaming_speech_) {
        FinishSpeechUtterance();
    } else if (listening_mode_ == kListeningModeManualStop && !pre_roll_.empty()) {
        EnqueueSilence16k(180);
        for (const auto& f : pre_roll_) {
            EnqueuePcm16k(f.data(), f.size());
        }
        pre_roll_.clear();
        awaiting_response_.store(true);
        awaiting_since_us_.store(esp_timer_get_time());
        awaiting_nudges_ = 0;
        EnqueueSilence16k(1100);
        EmitEmotion("thinking");
    }
}

void GeminiLiveProtocol::SendAbortSpeaking(AbortReason reason) {
    (void)reason;
    turn_gen_.fetch_add(1);
    drop_turn_.store(true);
    turn_complete_.store(false);
    awaiting_response_.store(false);
    speaking_active_.store(false);
    tool_hold_until_us_.store(0);
    ClearRing();
    ring_cv_.notify_all();
    Application::GetInstance().GetAudioService().ResetDecoder();
    EmitEmotion("v:0");
    EmitTtsState("stop");
}

void GeminiLiveProtocol::FlushPendingTranscripts() {
    std::string u;
    std::string b;
    {
        std::lock_guard<std::mutex> lk(transcript_mutex_);
        u.swap(pending_user_transcript_);
        b.swap(pending_bot_transcript_);
    }
    if (!u.empty()) {
        ESP_LOGI(TAG, "USER: %s", u.c_str());
        RobiConfig::GetInstance().AddDialogueTurn("user", u);
    }
    if (!b.empty()) {
        ESP_LOGI(TAG, "ROBI: %s", b.c_str());
        RobiConfig::GetInstance().AddDialogueTurn("assistant", b);
    }
}

void GeminiLiveProtocol::PushPcm24kToRing(const int16_t* samples, size_t count) {
    if (pcm_ring_ == nullptr || ring_capacity_ == 0 || count == 0) return;
    std::unique_lock<std::mutex> lk(ring_mutex_);
    for (size_t i = 0; i < count; ++i) {
        if (ring_count_ == ring_capacity_) {
            // Ring full: wait briefly for speaker to drain so we never drop audio!
            ring_cv_.wait_for(lk, std::chrono::milliseconds(60), [this]() {
                return stopping_.load() || drop_turn_.load() || ring_count_ < ring_capacity_;
            });
            if (stopping_.load() || drop_turn_.load()) return;
            if (ring_count_ == ring_capacity_) {
                // Drop oldest sample only as extreme last resort
                ring_head_ = (ring_head_ + 1) % ring_capacity_;
                --ring_count_;
            }
        }
        pcm_ring_[ring_tail_] = samples[i];
        ring_tail_ = (ring_tail_ + 1) % ring_capacity_;
        ++ring_count_;
    }
    lk.unlock();
    ring_cv_.notify_one();
}

size_t GeminiLiveProtocol::PopPcm24kFromRing(int16_t* out_samples, size_t max_count) {
    std::lock_guard<std::mutex> lk(ring_mutex_);
    size_t n = std::min(max_count, ring_count_);
    for (size_t i = 0; i < n; ++i) {
        out_samples[i] = pcm_ring_[ring_head_];
        ring_head_ = (ring_head_ + 1) % ring_capacity_;
    }
    ring_count_ -= n;
    return n;
}

size_t GeminiLiveProtocol::RingAvailable() const {
    std::lock_guard<std::mutex> lk(ring_mutex_);
    return ring_count_;
}

void GeminiLiveProtocol::ClearRing() {
    std::lock_guard<std::mutex> lk(ring_mutex_);
    ring_head_ = 0;
    ring_tail_ = 0;
    ring_count_ = 0;
    pending_emotions_.clear();
}

void GeminiLiveProtocol::EmitLocalJson(const std::string& json_str) {
    if (on_incoming_json_ == nullptr) return;
    cJSON* root = cJSON_ParseWithLength(json_str.data(), json_str.size());
    if (root != nullptr) {
        on_incoming_json_(root);
        cJSON_Delete(root);
    }
}

void GeminiLiveProtocol::EmitEmotion(const std::string& emotion) {
    std::string j = "{\"type\":\"llm\",\"emotion\":\"" + emotion + "\"}";
    EmitLocalJson(j);
}

void GeminiLiveProtocol::EmitTtsState(const char* state) {
    std::string j = std::string("{\"type\":\"tts\",\"state\":\"") + state + "\"}";
    EmitLocalJson(j);
}

int GeminiLiveProtocol::ComputeVoiceLevel(const int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0) return 0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double v = static_cast<double>(samples[i]);
        sum += v * v;
    }
    double rms = std::sqrt(sum / static_cast<double>(count));
    if (rms < 110.0) return 0;
    double norm = std::min(1.0, (rms - 100.0) / 4200.0);
    int lvl = static_cast<int>(std::lround(std::pow(norm, 0.72) * 100.0));
    return std::clamp(lvl, 0, 100);
}

void GeminiLiveProtocol::TxWorkerLoop() {
    while (!stopping_.load() && ws_connected_.load()) {
        std::string msg;
        {
            std::unique_lock<std::mutex> lk(tx_mutex_);
            tx_cv_.wait_for(lk, std::chrono::milliseconds(450), [this]() {
                return stopping_.load() || !ws_connected_.load() || !tx_queue_.empty();
            });
            if (stopping_.load() || !ws_connected_.load()) break;
            if (!tx_queue_.empty()) {
                msg = std::move(tx_queue_.front());
                tx_queue_.pop_front();
            }
        }

        if (!msg.empty()) {
            if (websocket_ && !websocket_->Send(msg)) {
                ESP_LOGW(TAG, "WebSocket send failed");
                break;
            }
            continue;
        }

        int64_t now_us = esp_timer_get_time();

        // While awaiting Gemini's response after an utterance, keep stream clock alive with gentle silence
        if (awaiting_response_.load() && RingAvailable() == 0 && !stopping_.load()) {
            int64_t waited_us = now_us - awaiting_since_us_.load();
            if (waited_us > 6500000LL) {
                // 6.5s timeout without modelTurn: unlock mic and return to Listening so Robi never stays stuck
                ESP_LOGW(TAG, "Awaiting response timed out after 6.5s; unlocking listening state");
                awaiting_response_.store(false);
                tool_hold_until_us_.store(0);
                if (Application::GetInstance().GetDeviceState() == kDeviceStateListening) {
                    EmitEmotion("listening");
                }
            } else if (waited_us > 1200000LL && awaiting_nudges_ < 4) {
                ++awaiting_nudges_;
                EnqueueSilence16k(480);
            }
        }

        // Periodic 90s idle check while channel is sitting quiet in Listening
        int64_t idle_us = now_us - last_activity_us_.load();
        if (!idle_farewell_sent_ && channel_opened_.load() && !awaiting_response_.load() &&
            Application::GetInstance().GetDeviceState() == kDeviceStateListening &&
            idle_us > 90LL * 1000000LL) {
            idle_farewell_sent_ = true;
            close_after_turn_.store(true);
            auto lang = RobiConfig::GetInstance().Get().language;
            std::string prompt =
                (lang == "en-US")
                    ? "[The child has been quiet for a while: say a warm 1-sentence goodbye and invite them to chat again soon.]"
                    : "[הילד שקט כבר זמן מה: היפרד ממנו בחום במשפט קצר אחד והזמן אותו לדבר שוב בקרוב.]";
            EnqueueClientTextTurn(prompt);
        }
    }
}

void GeminiLiveProtocol::RxWorkerLoop() {
    std::string clause_buf;
    bool clause_had_emoji = false;
    std::string last_emo;
    std::vector<uint8_t> pcm_bytes;
    pcm_bytes.reserve(16384);

    while (!stopping_.load()) {
        std::string raw;
        {
            std::unique_lock<std::mutex> lk(rx_mutex_);
            rx_cv_.wait(lk, [this]() {
                return stopping_.load() || !ws_connected_.load() || !rx_queue_.empty();
            });
            if (stopping_.load()) break;
            if (rx_queue_.empty() && !ws_connected_.load()) break;
            if (rx_queue_.empty()) continue;
            raw = std::move(rx_queue_.front());
            rx_queue_.pop_front();
        }

        cJSON* msg = cJSON_ParseWithLength(raw.data(), raw.size());
        if (msg == nullptr) {
            continue;
        }

        if (cJSON_HasObjectItem(msg, "setupComplete")) {
            xEventGroupSetBits(event_group_, kEvtSetupComplete);
            cJSON_Delete(msg);
            continue;
        }

        const cJSON* um = cJSON_GetObjectItemCaseSensitive(msg, "usageMetadata");
        if (cJSON_IsObject(um)) {
            const cJSON* tot_tok = cJSON_GetObjectItemCaseSensitive(um, "totalTokenCount");
            if (cJSON_IsNumber(tot_tok) && tot_tok->valueint > 0) {
                turn_server_tokens_.fetch_add(static_cast<uint32_t>(tot_tok->valueint));
            }
        }

        const cJSON* tool_call = cJSON_GetObjectItemCaseSensitive(msg, "toolCall");
        if (cJSON_IsObject(tool_call)) {
            HandleToolCalls(tool_call);
        }

        const cJSON* sc = cJSON_GetObjectItemCaseSensitive(msg, "serverContent");
        if (!cJSON_IsObject(sc)) {
            cJSON_Delete(msg);
            continue;
        }

        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(sc, "interrupted"))) {
            // In half-duplex turn-based mode, ignore server-side interruptions so ambient noise
            // or trailing silence packets never cut off Robi's speech or wipe transcripts!
            if (listening_mode_ != kListeningModeRealtime) {
                ESP_LOGI(TAG, "Ignoring server-side interrupted flag in turn-based mode");
                cJSON_Delete(msg);
                continue;
            }
            turn_gen_.fetch_add(1);
            drop_turn_.store(false);
            turn_complete_.store(false);
            ClearRing();
            ring_cv_.notify_all();
            {
                std::lock_guard<std::mutex> lk(transcript_mutex_);
                pending_user_transcript_.clear();
                pending_bot_transcript_.clear();
            }
            clause_buf.clear();
            clause_had_emoji = false;
            last_emo.clear();
            cJSON_Delete(msg);
            continue;
        }

        const cJSON* in_tr = cJSON_GetObjectItemCaseSensitive(sc, "inputTranscription");
        if (cJSON_IsObject(in_tr)) {
            const cJSON* t = cJSON_GetObjectItemCaseSensitive(in_tr, "text");
            if (cJSON_IsString(t) && t->valuestring != nullptr) {
                std::lock_guard<std::mutex> lk(transcript_mutex_);
                pending_user_transcript_ += t->valuestring;
            }
        }

        const cJSON* out_tr = cJSON_GetObjectItemCaseSensitive(sc, "outputTranscription");
        if (cJSON_IsObject(out_tr) && !drop_turn_.load()) {
            const cJSON* t = cJSON_GetObjectItemCaseSensitive(out_tr, "text");
            if (cJSON_IsString(t) && t->valuestring != nullptr) {
                std::string chunk(t->valuestring);
                {
                    std::lock_guard<std::mutex> lk(transcript_mutex_);
                    pending_bot_transcript_ += chunk;
                }
                const char* emo = MatchLeadingOrInlineEmoji(chunk);
                if (emo != nullptr) {
                    clause_had_emoji = true;
                    if (last_emo != emo) {
                        last_emo = emo;
                        std::lock_guard<std::mutex> lk(ring_mutex_);
                        pending_emotions_.emplace_back(emo);
                    }
                }
                clause_buf += chunk;
                if (chunk.find('.') != std::string::npos || chunk.find('!') != std::string::npos ||
                    chunk.find('?') != std::string::npos || chunk.find('\n') != std::string::npos) {
                    if (!clause_had_emoji) {
                        const char* inf = InferHebrewClauseEmotion(clause_buf);
                        if (inf != nullptr && last_emo != inf) {
                            last_emo = inf;
                            std::lock_guard<std::mutex> lk(ring_mutex_);
                            pending_emotions_.emplace_back(inf);
                        }
                    }
                    clause_buf.clear();
                    clause_had_emoji = false;
                }
            }
        }

        const cJSON* model_turn = cJSON_GetObjectItemCaseSensitive(sc, "modelTurn");
        if (cJSON_IsObject(model_turn) && !drop_turn_.load()) {
            const cJSON* parts = cJSON_GetObjectItemCaseSensitive(model_turn, "parts");
            if (cJSON_IsArray(parts)) {
                const cJSON* part = nullptr;
                cJSON_ArrayForEach(part, parts) {
                    const cJSON* inline_data = cJSON_GetObjectItemCaseSensitive(part, "inlineData");
                    if (!cJSON_IsObject(inline_data)) continue;
                    const cJSON* b64_item = cJSON_GetObjectItemCaseSensitive(inline_data, "data");
                    if (!cJSON_IsString(b64_item) || b64_item->valuestring == nullptr) continue;

                    size_t b64_len = std::strlen(b64_item->valuestring);
                    if (b64_len == 0) continue;

                    size_t max_decoded = (b64_len * 3) / 4 + 4;
                    if (pcm_bytes.size() < max_decoded) {
                        pcm_bytes.resize(max_decoded);
                    }
                    size_t decoded_len = 0;
                    if (mbedtls_base64_decode(
                            pcm_bytes.data(), pcm_bytes.size(), &decoded_len,
                            reinterpret_cast<const unsigned char*>(b64_item->valuestring),
                            b64_len) == 0 &&
                        decoded_len >= 2) {
                        int64_t now_rx_us = esp_timer_get_time();
                        last_activity_us_.store(now_rx_us);
                        last_rx_audio_us_.store(now_rx_us);
                        // Keep awaiting_response_ = true until PlaybackFeederLoop sets speaking_active_ = true
                        // so SendAudio() can never open the mic during the pre-buffer window!
                        tool_hold_until_us_.store(0);
                        turn_complete_.store(false);
                        size_t rx_samples = decoded_len / sizeof(int16_t);
                        turn_downlink_samples_.fetch_add(static_cast<uint32_t>(rx_samples));
                        PushPcm24kToRing(reinterpret_cast<const int16_t*>(pcm_bytes.data()),
                                         rx_samples);
                    }
                }
            }
        }

        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(sc, "turnComplete"))) {
            FlushPendingTranscripts();
            clause_buf.clear();
            clause_had_emoji = false;
            last_emo.clear();

            if (drop_turn_.exchange(false)) {
                ClearRing();
                turn_complete_.store(false);
            } else {
                turn_complete_.store(true);
                ring_cv_.notify_all();
            }
        }

        cJSON_Delete(msg);
    }
}

void GeminiLiveProtocol::PlaybackFeederLoop() {
    std::vector<int16_t> frame_buf(kPlaybackFrameSamples);
    bool speaking_active = false;
    bool rebuffer_needed = false;
    int initial_burst_left = 0;
    uint32_t active_gen = 0;
    int frames_since_env = 0;
    int last_env_lvl = -1;

    while (!stopping_.load()) {
        size_t min_needed = speaking_active
                                ? (rebuffer_needed ? kRebufferSamples : kPlaybackFrameSamples)
                                : kPreBufferSamples;
        {
            std::unique_lock<std::mutex> lk(ring_mutex_);
            ring_cv_.wait_for(lk, std::chrono::milliseconds(35), [this, min_needed, speaking_active]() {
                if (stopping_.load() || !ws_connected_.load()) return true;
                if (drop_turn_.load()) return true;
                if (turn_complete_.load() && (speaking_active || ring_count_ > 0)) return true;
                if (ring_count_ > 0 && (esp_timer_get_time() - last_rx_audio_us_.load()) > 400000LL) return true;
                return ring_count_ >= min_needed;
            });
        }

        if (stopping_.load() || !ws_connected_.load()) break;

        if (drop_turn_.exchange(false)) {
            if (speaking_active) {
                EmitEmotion("v:0");
                EmitTtsState("stop");
                speaking_active = false;
                speaking_active_.store(false);
            }
            rebuffer_needed = false;
            continue;
        }

        size_t avail = RingAvailable();
        int64_t idle_rx_us = esp_timer_get_time() - last_rx_audio_us_.load();
        if (!speaking_active) {
            if (avail >= kPreBufferSamples || ((turn_complete_.load() || idle_rx_us > 400000LL) && avail > 0)) {
                speaking_active = true;
                speaking_active_.store(true);
                rebuffer_needed = false;
                initial_burst_left = 6;  // Immediately preload 300ms (6 * 50ms) into AudioService queue
                active_gen = turn_gen_.load();
                frames_since_env = 0;
                last_env_lvl = -1;
                awaiting_response_.store(false);
                EmitTtsState("start");

                // Wait up to 450ms for Application main loop to enter kDeviceStateSpeaking so 0 frames are dropped
                int wait_spk = 0;
                while (!stopping_.load() && active_gen == turn_gen_.load() && !drop_turn_.load() &&
                       Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking &&
                       wait_spk < 45) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    ++wait_spk;
                }
            } else {
                // If turnComplete arrived with 0 audio samples (and we aren't holding for a tool call), clear it!
                if (turn_complete_.load() && avail == 0 && esp_timer_get_time() >= tool_hold_until_us_.load()) {
                    FlushPendingTranscripts();
                    turn_complete_.store(false);
                    awaiting_response_.store(false);
                    if (Application::GetInstance().GetDeviceState() == kDeviceStateListening) {
                        EmitEmotion("listening");
                    }
                }
                continue;
            }
        }

        if (active_gen != turn_gen_.load() || drop_turn_.exchange(false)) {
            EmitEmotion("v:0");
            EmitTtsState("stop");
            speaking_active = false;
            speaking_active_.store(false);
            rebuffer_needed = false;
            continue;
        }

        // Fire any queued emotion synced with audio playback
        {
            std::string emo;
            {
                std::lock_guard<std::mutex> lk(ring_mutex_);
                if (!pending_emotions_.empty()) {
                    emo = std::move(pending_emotions_.front());
                    pending_emotions_.pop_front();
                }
            }
            if (!emo.empty()) {
                EmitEmotion(emo);
            }
        }

        avail = RingAvailable();
        idle_rx_us = esp_timer_get_time() - last_rx_audio_us_.load();
        size_t pop_threshold = rebuffer_needed ? kRebufferSamples : kPlaybackFrameSamples;
        if (avail >= pop_threshold || ((turn_complete_.load() || idle_rx_us > 400000LL) && avail > 0)) {
            if (rebuffer_needed) {
                rebuffer_needed = false;
                initial_burst_left = 4;  // Preload 200ms after mid-turn rebuffer
            }
            size_t to_pop = std::min(kPlaybackFrameSamples, avail);
            size_t got = PopPcm24kFromRing(frame_buf.data(), to_pop);
            ring_cv_.notify_one();

            if (got > 0) {
                audio_frames_played_.fetch_add(1);
                ++frames_since_env;
                if (frames_since_env >= 5) {
                    frames_since_env = 0;
                    int lvl = ComputeVoiceLevel(frame_buf.data(), got);
                    if (std::abs(lvl - last_env_lvl) >= 12 || (lvl == 0 && last_env_lvl != 0)) {
                        last_env_lvl = lvl;
                        EmitEmotion("v:" + std::to_string(lvl));
                    }
                }

                auto pkt = std::make_unique<AudioStreamPacket>();
                pkt->sample_rate = 24000;
                pkt->frame_duration = 0;  // 0 = raw 24kHz int16 PCM bypass in AudioService!
                pkt->payload.resize(got * sizeof(int16_t));
                std::memcpy(pkt->payload.data(), frame_buf.data(), pkt->payload.size());

                auto& as = Application::GetInstance().GetAudioService();
                while (!stopping_.load() && active_gen == turn_gen_.load() && !drop_turn_.load()) {
                    if (Application::GetInstance().GetDeviceState() != kDeviceStateSpeaking) {
                        break;
                    }
                    if (as.PushPacketToDecodeQueue(std::move(pkt), false)) {
                        break;
                    }
                    // AudioService queue is full (40 packets = 2.0s buffered); wait for hardware DMA to drain
                    pkt = std::make_unique<AudioStreamPacket>();
                    pkt->sample_rate = 24000;
                    pkt->frame_duration = 0;
                    pkt->payload.resize(got * sizeof(int16_t));
                    std::memcpy(pkt->payload.data(), frame_buf.data(), pkt->payload.size());
                    vTaskDelay(pdMS_TO_TICKS(18));
                }

                if (initial_burst_left > 0) {
                    --initial_burst_left;
                } else {
                    // Feed 50ms frames every 40ms so AudioService queue stays topped up and never underruns
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
            }
            continue;
        }

        // If a tool call (like save_memory) was just answered, hold speaking state until post-tool audio arrives!
        if (esp_timer_get_time() < tool_hold_until_us_.load()) {
            turn_complete_.store(false);
            rebuffer_needed = true;
            continue;
        }

        // Ring has fewer than kPlaybackFrameSamples
        if (!turn_complete_.load()) {
            auto& as = Application::GetInstance().GetAudioService();
            if (as.IsPlaybackIdle()) {
                if (avail == 0 && idle_rx_us > 1600000LL) {
                    ESP_LOGW(TAG, "Synthesizing turnComplete after 1.6s audio stream idle");
                    turn_complete_.store(true);
                } else {
                    // Hardware queue drained mid-turn due to network jitter; accumulate cushion before resuming
                    rebuffer_needed = true;
                    continue;
                }
            } else {
                continue;
            }
        }

        // Turn is complete and ring is completely drained; wait for hardware I2S DMA playback queue to drain
        auto& as = Application::GetInstance().GetAudioService();
        int wait_loops = 0;
        bool more_audio_arrived = false;
        while (!stopping_.load() && active_gen == turn_gen_.load() &&
               !as.IsPlaybackIdle() && wait_loops < 100) {
            if (RingAvailable() > 0 || esp_timer_get_time() < tool_hold_until_us_.load()) {
                more_audio_arrived = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(25));
            ++wait_loops;
        }
        if (more_audio_arrived) {
            turn_complete_.store(false);
            continue;
        }

        FlushPendingTranscripts();
        turn_complete_.store(false);
        speaking_active = false;
        speaking_active_.store(false);
        rebuffer_needed = false;

        if (!stopping_.load() && active_gen == turn_gen_.load()) {
            turns_completed_.fetch_add(1);
            uint32_t up_bytes = turn_uplink_pcm_bytes_.exchange(0);
            uint32_t down_samples = turn_downlink_samples_.exchange(0);
            uint32_t srv_tok = turn_server_tokens_.exchange(0);
            uint32_t up_sec = (up_bytes + 16000) / 32000;
            uint32_t down_sec = (down_samples + 12000) / 24000;
            uint32_t audio_sec_delta = std::max<uint32_t>(1, up_sec + down_sec);
            if (srv_tok == 0) {
                srv_tok = audio_sec_delta * 35 + 140;
            }
            RobiConfig::GetInstance().RecordModelTurn(GetActiveModel(), audio_sec_delta, srv_tok);
            awaiting_response_.store(false);
            EmitEmotion("v:0");
            last_activity_us_.store(esp_timer_get_time());
            EmitTtsState("stop");

            if (close_after_turn_.exchange(false)) {
                vTaskDelay(pdMS_TO_TICKS(250));
                Application::GetInstance().Schedule([this]() {
                    if (IsAudioChannelOpened()) {
                        CloseAudioChannel(false);
                    }
                });
            }
        }
    }
}

void GeminiLiveProtocol::HandleToolCalls(const cJSON* tool_call) {
    const cJSON* fcalls = cJSON_GetObjectItemCaseSensitive(tool_call, "functionCalls");
    if (!cJSON_IsArray(fcalls)) return;

    cJSON* root = cJSON_CreateObject();
    cJSON* tr = cJSON_CreateObject();
    cJSON* f_resps = cJSON_CreateArray();
    bool expect_continuation = false;

    const cJSON* fc = nullptr;
    cJSON_ArrayForEach(fc, fcalls) {
        const cJSON* id_it = cJSON_GetObjectItemCaseSensitive(fc, "id");
        const cJSON* name_it = cJSON_GetObjectItemCaseSensitive(fc, "name");
        const cJSON* args = cJSON_GetObjectItemCaseSensitive(fc, "args");
        if (!cJSON_IsString(name_it) || name_it->valuestring == nullptr) continue;

        std::string fc_id = (cJSON_IsString(id_it) && id_it->valuestring) ? id_it->valuestring : "";
        std::string name = name_it->valuestring;
        ESP_LOGI(TAG, "Gemini tool call: %s", name.c_str());

        cJSON* res_obj = cJSON_CreateObject();
        if (name == "handle_exit_intent") {
            close_after_turn_.store(true);
            cJSON_AddStringToObject(res_obj, "result", "session_will_close_after_turn");
        } else {
            expect_continuation = true;
            if (name == "set_volume") {
                int vol = 70;
                const cJSON* v = args ? cJSON_GetObjectItemCaseSensitive(args, "volume") : nullptr;
                if (cJSON_IsNumber(v)) vol = std::clamp(v->valueint, 0, 100);
                // Schedule onto Internal SRAM main task so NVS inside SetOutputVolume never panics on PSRAM stack
                Application::GetInstance().Schedule([vol]() {
                    if (auto* codec = Board::GetInstance().GetAudioCodec()) {
                        codec->SetOutputVolume(vol);
                    }
                });
                cJSON_AddNumberToObject(res_obj, "volume", vol);
                cJSON_AddStringToObject(res_obj, "result", "ok");
            } else if (name == "set_brightness") {
                int bri = 80;
                const cJSON* b = args ? cJSON_GetObjectItemCaseSensitive(args, "brightness") : nullptr;
                if (cJSON_IsNumber(b)) bri = std::clamp(b->valueint, 10, 100);
                // Schedule onto Internal SRAM main task so NVS inside SetBrightness never panics on PSRAM stack
                Application::GetInstance().Schedule([bri]() {
                    if (auto* bl = Board::GetInstance().GetBacklight()) {
                        bl->SetBrightness(static_cast<uint8_t>(bri), true);
                    }
                });
                cJSON_AddNumberToObject(res_obj, "brightness", bri);
                cJSON_AddStringToObject(res_obj, "result", "ok");
            } else if (name == "get_device_status") {
                std::string st = Board::GetInstance().GetDeviceStatusJson();
                cJSON_AddStringToObject(res_obj, "status", st.c_str());
            } else if (name == "save_memory") {
                const cJSON* n = args ? cJSON_GetObjectItemCaseSensitive(args, "note") : nullptr;
                if (cJSON_IsString(n) && n->valuestring != nullptr) {
                    RobiConfig::GetInstance().AddMemoryNote(n->valuestring);
                    cJSON_AddStringToObject(res_obj, "result", "saved");
                } else {
                    cJSON_AddStringToObject(res_obj, "result", "ignored");
                }
            } else {
                cJSON_AddStringToObject(res_obj, "result", "unknown_tool");
            }
        }

        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", fc_id.c_str());
        cJSON_AddStringToObject(item, "name", name.c_str());
        cJSON_AddItemToObject(item, "response", res_obj);
        cJSON_AddItemToArray(f_resps, item);
    }

    if (expect_continuation) {
        int64_t now_us = esp_timer_get_time();
        tool_hold_until_us_.store(now_us + 3500000LL);
        awaiting_response_.store(true);
        awaiting_since_us_.store(now_us);
        awaiting_nudges_ = 0;
        turn_complete_.store(false);
    }

    cJSON_AddItemToObject(tr, "functionResponses", f_resps);
    cJSON_AddItemToObject(root, "toolResponse", tr);
    char* printed = cJSON_PrintUnformatted(root);
    if (printed != nullptr) {
        EnqueueTxJson(std::string(printed));
        cJSON_free(printed);
    }
    cJSON_Delete(root);
}
