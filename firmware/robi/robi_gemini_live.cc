#include "robi_gemini_live.h"
#include "robi_config.h"
#include "robi_utf8.h"

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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
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
    int rc = 0;
    if (cfg != nullptr) {
        esp_tls_cfg_t patched = *cfg;
        if (patched.timeout_ms <= 0) {
            patched.timeout_ms = 8000;
        }
        rc = __real_esp_tls_conn_new_sync(hostname, hostlen, port, &patched, tls);
    } else {
        rc = __real_esp_tls_conn_new_sync(hostname, hostlen, port, cfg, tls);
    }
    if (rc == 1 && tls != nullptr) {
        int sockfd = -1;
        if (esp_tls_get_conn_sockfd(tls, &sockfd) == ESP_OK && sockfd >= 0) {
            // Disable Nagle's algorithm so 64ms audio chunks, audioStreamEnd, and toolResponses
            // never stall 65-200ms waiting for a prior TCP ACK over WiFi.
            int nodelay = 1;
            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
            // Tag packets for 802.11e WMM low-latency Voice/Video queue on home routers.
            int tos = 0xA0;
            setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
        }
    }
    return rc;
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

constexpr size_t kPsramRingSamples = 262144;    // 512 KB = 10.92s @ 24kHz
constexpr size_t kFallbackRingSamples = 48000;
constexpr size_t kPreBufferSamples = 4800;      // 200ms @ 24kHz cushion before starting speaker
constexpr size_t kRebufferSamples = 4800;       // 200ms @ 24kHz cushion if network stalls mid-turn
constexpr size_t kPlaybackFrameSamples = 1200;  // 50ms @ 24kHz (divides 2400/4800 evenly)
constexpr size_t kUplinkBatchBytes = 2048;      // 64ms @ 16kHz int16 (2 AFE frames)
constexpr int64_t kPostPlaybackGuardUs = 95000LL;  // 95ms room-reverb settle after DMA idle

std::atomic<int64_t> g_model_cooldown_until_us[3] = {{0}, {0}, {0}};
std::mutex g_last_model_mutex;
std::string g_last_connected_model;
GeminiLiveProtocol* g_active_protocol = nullptr;

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
    tx_pcm_batch_.reserve(4096);

    pcm_ring_mem_ = static_cast<int16_t*>(
        heap_caps_malloc(kPsramRingSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcm_ring_mem_ != nullptr) {
        jitter_ring_.AttachBuffer(pcm_ring_mem_, kPsramRingSamples);
    } else {
        pcm_ring_mem_ = static_cast<int16_t*>(malloc(kFallbackRingSamples * sizeof(int16_t)));
        jitter_ring_.AttachBuffer(pcm_ring_mem_, pcm_ring_mem_ ? kFallbackRingSamples : 0);
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
    if (pcm_ring_mem_ != nullptr) {
        jitter_ring_.AttachBuffer(nullptr, 0);
        heap_caps_free(pcm_ring_mem_);
        pcm_ring_mem_ = nullptr;
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

void GeminiLiveProtocol::NotifySpeakingAudioReady() {
    speaking_audio_ready_.store(true, std::memory_order_release);
    jitter_ring_.NotifyAll();
}

void GeminiLiveProtocol::ClearLiveVoiceLevel() {
    live_voice_level_.store(0, std::memory_order_relaxed);
    live_voice_updated_us_.store(0, std::memory_order_relaxed);
}

int GeminiLiveProtocol::GetLiveVoiceLevel(int64_t now_us) const {
    int64_t ts = live_voice_updated_us_.load(std::memory_order_relaxed);
    if (ts <= 0 || (now_us - ts) > 350000LL) {
        return -1;
    }
    return live_voice_level_.load(std::memory_order_relaxed);
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
    int64_t until_us = g_model_cooldown_until_us[idx].load(std::memory_order_relaxed);
    if (until_us <= now_us) return 0;
    return static_cast<int>((until_us - now_us + 999999LL) / 1000000LL);
}

void GeminiLiveProtocol::ResetModelCooldowns() {
    for (int i = 0; i < 3; ++i) {
        g_model_cooldown_until_us[i].store(0, std::memory_order_relaxed);
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

void GeminiLiveProtocol::ResetTurnRuntimeFlags() {
    awaiting_response_.store(false);
    receiving_model_audio_.store(false);
    speaking_active_.store(false);
    speaking_audio_ready_.store(false);
    streaming_speech_.store(false);
    user_turn_had_speech_.store(false);
    tool_hold_until_us_.store(0);
    tool_sent_at_us_.store(0);
    awaiting_nudges_.store(0);
    ClearLiveVoiceLevel();
}

void GeminiLiveProtocol::StopSessionInternal(bool notify_closed) {
    stopping_.store(true);
    channel_opened_.store(false);
    ws_connected_.store(false);
    ResetTurnRuntimeFlags();
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

    jitter_ring_.Clear();
    jitter_ring_.NotifyAll();

    if (rx_task_ != nullptr || tx_task_ != nullptr || play_task_ != nullptr) {
        xEventGroupWaitBits(worker_done_group_, kDoneAll, pdFALSE, pdTRUE, pdMS_TO_TICKS(9500));
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

void GeminiLiveProtocol::HandleFatalSocketError(const char* reason) {
    if (stopping_.load()) return;
    ESP_LOGW(TAG, "Closing broken Gemini Live session (%s)", reason ? reason : "socket_error");
    ws_connected_.store(false);
    ResetTurnRuntimeFlags();
    xEventGroupSetBits(event_group_, kEvtWsClosed);
    rx_cv_.notify_all();
    tx_cv_.notify_all();
    jitter_ring_.NotifyAll();
    if (channel_opened_.exchange(false) && !stopping_.load()) {
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
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
    cJSON_AddNumberToObject(aad, "silenceDurationMs", 420);
    cJSON_AddItemToObject(rt_cfg, "automaticActivityDetection", aad);
    cJSON_AddItemToObject(setup, "realtimeInputConfig", rt_cfg);

    cJSON* sys_inst = cJSON_CreateObject();
    cJSON* sys_parts = cJSON_CreateArray();
    cJSON* sys_part0 = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_part0, "text", sys_prompt.c_str());
    cJSON_AddItemToArray(sys_parts, sys_part0);
    cJSON_AddItemToObject(sys_inst, "parts", sys_parts);
    cJSON_AddItemToObject(setup, "systemInstruction", sys_inst);

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

    {
        cJSON* fn = cJSON_CreateObject();
        cJSON_AddStringToObject(fn, "name", "save_memory");
        cJSON_AddStringToObject(
            fn, "description",
            "Save a short note (up to 200 chars) ONLY when discovering a meaningful new personal fact about the child (family, friends, hobbies, curiosities, fears) or reaching a story chapter milestone. Do NOT call on routine math/quiz answers. Call AFTER finishing your spoken reply.");
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

std::string GeminiLiveProtocol::BuildClientTurnJson(const std::string& text) {
    std::string safe_text = robi::Utf8Sanitize(text);
    cJSON* root = cJSON_CreateObject();
    cJSON* cc = cJSON_CreateObject();
    cJSON* turns = cJSON_CreateArray();
    cJSON* turn0 = cJSON_CreateObject();
    cJSON_AddStringToObject(turn0, "role", "user");
    cJSON* parts = cJSON_CreateArray();
    cJSON* p0 = cJSON_CreateObject();
    cJSON_AddStringToObject(p0, "text", safe_text.c_str());
    cJSON_AddItemToArray(parts, p0);
    cJSON_AddItemToObject(turn0, "parts", parts);
    cJSON_AddItemToArray(turns, turn0);
    cJSON_AddItemToObject(cc, "turns", turns);
    cJSON_AddBoolToObject(cc, "turnComplete", true);
    cJSON_AddItemToObject(root, "clientContent", cc);

    char* s = cJSON_PrintUnformatted(root);
    std::string out = s ? s : "";
    if (s != nullptr) cJSON_free(s);
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
    ResetTurnRuntimeFlags();
    silent_nudge_used_.store(false);
    consecutive_timeouts_.store(0);
    awaiting_since_us_.store(0);
    post_playback_guard_until_us_.store(0);
    last_input_tr_us_.store(0);
    int64_t boot_now_us = esp_timer_get_time();
    last_rx_audio_us_.store(boot_now_us);
    idle_farewell_sent_ = false;
    last_activity_us_.store(boot_now_us);
    {
        std::lock_guard<std::mutex> ulk(uplink_mutex_);
        tx_pcm_batch_.clear();
        speech_tracker_.Reset();
    }
    {
        std::lock_guard<std::mutex> clk(client_turn_mutex_);
        last_client_turn_text_.clear();
    }
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
            if (rx_queue_.size() < 256) {
                rx_queue_.emplace_back(data, len);
            }
        }
        rx_cv_.notify_one();
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGW(TAG, "Gemini Live WebSocket disconnected");
        ws_connected_.store(false);
        ResetTurnRuntimeFlags();
        xEventGroupSetBits(event_group_, kEvtWsClosed);
        rx_cv_.notify_all();
        tx_cv_.notify_all();
        jitter_ring_.NotifyAll();
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
        if (WifiManager::GetInstance().GetIpAddress().empty()) {
            ESP_LOGW(TAG, "WiFi has no IP address; aborting TLS connect");
            break;
        }
        auto connected = websocket_->Connect(url.c_str());
        if (connected) {
            ws_ok = true;
            break;
        }
        ESP_LOGW(TAG, "Connect attempt %d failed (%s): %s", attempt + 1, model_name.c_str(),
                 connected.error().ToString().c_str());
        if (attempt == 0) {
            vTaskDelay(pdMS_TO_TICKS(250));
        }
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
                                           pdFALSE, pdMS_TO_TICKS(7500));
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
    bool tried_in_first_pass[3] = {false, false, false};

    for (const auto& c : ordered) {
        if (now_us < g_model_cooldown_until_us[c.idx].load(std::memory_order_relaxed)) {
            continue;
        }
        tried_in_first_pass[c.idx] = true;
        if (TryConnectModel(cfg.gemini_key, c.model, cfg.voice, cfg.language, c.with_lang)) {
            g_model_cooldown_until_us[c.idx].store(0, std::memory_order_relaxed);
            return true;
        }
        if (WifiManager::GetInstance().GetIpAddress().empty()) {
            break;
        }
        g_model_cooldown_until_us[c.idx].store(esp_timer_get_time() + 600LL * 1000000LL,
                                               std::memory_order_relaxed);
    }

    // Only retry candidates that were skipped due to a prior cooldown (never double-try one that just failed!)
    if (!WifiManager::GetInstance().GetIpAddress().empty()) {
        for (const auto& c : ordered) {
            if (tried_in_first_pass[c.idx]) continue;
            g_model_cooldown_until_us[c.idx].store(0, std::memory_order_relaxed);
            if (TryConnectModel(cfg.gemini_key, c.model, cfg.voice, cfg.language, c.with_lang)) {
                return true;
            }
            g_model_cooldown_until_us[c.idx].store(esp_timer_get_time() + 600LL * 1000000LL,
                                                   std::memory_order_relaxed);
        }
    }

    SetError(Lang::Strings::SERVER_NOT_CONNECTED);
    return false;
}

bool GeminiLiveProtocol::SendText(const std::string& text) {
    if (!ws_connected_.load()) return false;
    EnqueueTxJson(robi::Utf8Sanitize(text));
    return true;
}

void GeminiLiveProtocol::EnqueueTxJson(std::string&& json_str, bool is_audio) {
    if (stopping_.load() || !ws_connected_.load() || json_str.empty()) return;
    {
        std::lock_guard<std::mutex> lk(tx_mutex_);
        if (is_audio && tx_queue_.size() >= 52) {
            // Drop oldest 64ms audio chunk if WiFi stalls >3.3s so uplink latency never snowballs
            tx_queue_.pop_front();
        } else if (tx_queue_.size() >= 140) {
            tx_queue_.pop_front();
        }
        tx_queue_.push_back(std::move(json_str));
    }
    tx_cv_.notify_one();
}

void GeminiLiveProtocol::EnqueuePcm16k(const uint8_t* pcm_bytes, size_t byte_len) {
    if (pcm_bytes == nullptr || byte_len == 0 || stopping_.load() || !ws_connected_.load()) return;
    for (size_t off = 0; off < byte_len; off += kUplinkBatchBytes) {
        size_t n = std::min(kUplinkBatchBytes, byte_len - off);
        std::string b64 = Base64Encode(pcm_bytes + off, n);
        if (b64.empty()) continue;
        std::string msg;
        msg.reserve(b64.size() + 80);
        msg += "{\"realtimeInput\":{\"audio\":{\"mimeType\":\"audio/pcm;rate=16000\",\"data\":\"";
        msg += b64;
        msg += "\"}}}";
        EnqueueTxJson(std::move(msg), /*is_audio=*/true);
    }
}

void GeminiLiveProtocol::EnqueueSilence16k(size_t duration_ms) {
    size_t total_bytes = (duration_ms * 16000 / 1000) * sizeof(int16_t);
    std::vector<uint8_t> zeros(std::min<size_t>(total_bytes, kUplinkBatchBytes), 0);
    size_t rem = total_bytes;
    while (rem > 0) {
        size_t chunk = std::min(rem, zeros.size());
        EnqueuePcm16k(zeros.data(), chunk);
        rem -= chunk;
    }
}

void GeminiLiveProtocol::EnqueueAudioStreamEnd() {
    EnqueueTxJson("{\"realtimeInput\":{\"audioStreamEnd\":true}}", /*is_audio=*/false);
}

bool GeminiLiveProtocol::CanExitListeningByTap() const {
    int64_t started = listen_started_us_.load();
    if (started <= 0) return true;
    return (esp_timer_get_time() - started) > 850000LL;
}

void GeminiLiveProtocol::FinishSpeechUtterance() {
    streaming_speech_.store(false);
    speech_tracker_.Reset();
    if (!tx_pcm_batch_.empty()) {
        EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
        tx_pcm_batch_.clear();
    }
    drop_turn_.store(false);
    turn_downlink_samples_.store(0);
    user_turn_had_speech_.store(true);
    silent_nudge_used_.store(false);
    awaiting_response_.store(true);
    awaiting_since_us_.store(esp_timer_get_time());
    awaiting_nudges_.store(0);
    EnqueueSilence16k(192);
    EnqueueAudioStreamEnd();
}

bool GeminiLiveProtocol::CommitListeningUtterance() {
    std::lock_guard<std::mutex> lk(uplink_mutex_);
    if (!IsAudioChannelOpened()) return false;
    FinishSpeechUtterance();
    return true;
}

bool GeminiLiveProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!IsAudioChannelOpened() || !packet || packet->payload.empty()) {
        return false;
    }
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateListening) {
        return true;
    }

    int64_t now_us = esp_timer_get_time();
    // Hold mic while speaker is playing (e.g. OGG_POPUP chime) + 95ms room reverb settle window
    if (!app.GetAudioService().IsPlaybackIdle()) {
        post_playback_guard_until_us_.store(now_us + kPostPlaybackGuardUs);
        return true;
    }
    if (now_us < post_playback_guard_until_us_.load()) {
        return true;
    }

    // Hold mic while Gemini is streaming/playing an answer or answering an explicit clientContent/tool turn
    if (awaiting_response_.load() || receiving_model_audio_.load() || speaking_active_.load() ||
        jitter_ring_.Available() > 0 || now_us < tool_hold_until_us_.load()) {
        return true;
    }

    std::lock_guard<std::mutex> lk(uplink_mutex_);

    auto* samples = reinterpret_cast<int16_t*>(packet->payload.data());
    size_t sample_count = packet->payload.size() / sizeof(int16_t);
    if (sample_count == 0) return true;

    // Fast integer DC-bias removal + single float sqrtf so Gemini Live's neural VAD gets clean zero-mean PCM
    float ac_rms = robi::RemoveDcAndComputeRms(samples, sample_count);
    uint32_t frame_ms = std::max<uint32_t>(10u, static_cast<uint32_t>((sample_count * 1000u) / 16000u));

    auto speech_dec = speech_tracker_.OnFrame(ac_rms, app.IsVoiceDetected(), frame_ms);
    streaming_speech_.store(speech_dec.active_voice);
    if (speech_dec.active_voice) {
        idle_farewell_sent_ = false;
        last_activity_us_.store(now_us);
        turn_uplink_pcm_bytes_.fetch_add(static_cast<uint32_t>(packet->payload.size()));
    }

    // Stream continuous 64ms (2048-byte) 16kHz PCM batches directly to Gemini Live's server-side neural VAD.
    // Never gate or lock the mic locally — if background AC/TV hum ever prevents Google's 420ms VAD from
    // closing after clear user speech, `should_send_soft_stream_end` emits a non-locking `audioStreamEnd` backstop!
    drop_turn_.store(false);
    tx_pcm_batch_.insert(tx_pcm_batch_.end(), packet->payload.begin(), packet->payload.end());
    if (tx_pcm_batch_.size() >= kUplinkBatchBytes) {
        EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
        tx_pcm_batch_.clear();
    }

    if (speech_dec.should_send_soft_stream_end && !receiving_model_audio_.load() &&
        !speaking_active_.load() && jitter_ring_.Available() == 0) {
        if (!tx_pcm_batch_.empty()) {
            EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
            tx_pcm_batch_.clear();
        }
        ESP_LOGI(TAG, "Soft end-of-speech stream flush after 1.05s quiet (noise_floor=%.0f)",
                 static_cast<double>(speech_tracker_.noise_floor_rms()));
        EnqueueAudioStreamEnd();
    }

    return true;
}

void GeminiLiveProtocol::EnqueueClientTextTurn(const std::string& text) {
    std::string clean_text = robi::Utf8Sanitize(text);
    if (clean_text.empty()) return;
    ESP_LOGI(TAG, "Client text turn: %s", clean_text.c_str());

    int64_t now_us = esp_timer_get_time();
    drop_turn_.store(false);
    turn_complete_.store(false);
    turn_downlink_samples_.store(0);
    user_turn_had_speech_.store(false);
    awaiting_response_.store(true);
    awaiting_since_us_.store(now_us);
    awaiting_nudges_.store(0);
    last_activity_us_.store(now_us);
    {
        std::lock_guard<std::mutex> clk(client_turn_mutex_);
        last_client_turn_text_ = clean_text;
    }
    RobiConfig::GetInstance().AddDialogueTurn("user", clean_text);
    EmitEmotion("thinking");

    std::string payload = BuildClientTurnJson(clean_text);
    if (!payload.empty()) {
        EnqueueTxJson(std::move(payload));
    }
}

void GeminiLiveProtocol::EnqueueSilentTurnNudge(const std::string& user_text) {
    std::string clean_text = robi::Utf8Sanitize(user_text);
    if (clean_text.empty()) return;

    int64_t now_us = esp_timer_get_time();
    drop_turn_.store(false);
    turn_complete_.store(false);
    turn_downlink_samples_.store(0);
    user_turn_had_speech_.store(false);
    awaiting_response_.store(true);
    awaiting_since_us_.store(now_us);
    awaiting_nudges_.store(1);
    last_activity_us_.store(now_us);
    {
        std::lock_guard<std::mutex> clk(client_turn_mutex_);
        last_client_turn_text_ = clean_text;
    }
    EmitEmotion("thinking");

    std::string payload = BuildClientTurnJson(clean_text);
    if (!payload.empty()) {
        EnqueueTxJson(std::move(payload));
    }
}

void GeminiLiveProtocol::SendWakeWordDetected(const std::string& wake_word) {
    if (wake_word.empty() || robi::IsWakeWordToken(wake_word)) {
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
            vTaskDelay(pdMS_TO_TICKS(140));
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
            for (int f = 0; f < 36; ++f) {
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
    tx_pcm_batch_.clear();
    speech_tracker_.Reset();
    streaming_speech_.store(false);
    receiving_model_audio_.store(false);
    int64_t now_us = esp_timer_get_time();
    // Preserve awaiting_response_ when an activity-button clientContent turn was just submitted
    if (!awaiting_response_.load() || (now_us - awaiting_since_us_.load()) > 5500000LL) {
        awaiting_response_.store(false);
        awaiting_nudges_.store(0);
    }
    listen_started_us_.store(now_us);
    post_playback_guard_until_us_.store(now_us + kPostPlaybackGuardUs);
    last_activity_us_.store(now_us);
}

void GeminiLiveProtocol::SendStopListening() {
    std::lock_guard<std::mutex> lk(uplink_mutex_);
    if (listening_mode_ == kListeningModeManualStop) {
        FinishSpeechUtterance();
    } else if (!tx_pcm_batch_.empty()) {
        EnqueuePcm16k(tx_pcm_batch_.data(), tx_pcm_batch_.size());
        tx_pcm_batch_.clear();
    }
}

void GeminiLiveProtocol::SendAbortSpeaking(AbortReason reason) {
    (void)reason;
    turn_gen_.fetch_add(1);
    drop_turn_.store(true);
    turn_complete_.store(false);
    ResetTurnRuntimeFlags();
    jitter_ring_.Clear();
    jitter_ring_.NotifyAll();
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
    u = robi::CleanTranscriptText(u);
    b = robi::CleanTranscriptText(b);
    if (!u.empty()) {
        ESP_LOGI(TAG, "USER: %s", u.c_str());
        RobiConfig::GetInstance().AddDialogueTurn("user", u);
    }
    if (!b.empty()) {
        ESP_LOGI(TAG, "ROBI: %s", b.c_str());
        RobiConfig::GetInstance().AddDialogueTurn("assistant", b);
    }
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

void GeminiLiveProtocol::TxWorkerLoop() {
    while (!stopping_.load() && ws_connected_.load()) {
        std::string msg;
        {
            std::unique_lock<std::mutex> lk(tx_mutex_);
            tx_cv_.wait_for(lk, std::chrono::milliseconds(150), [this]() {
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
                HandleFatalSocketError("ws_send_failed");
                break;
            }
        }

        int64_t now_us = esp_timer_get_time();

        // 1. Proactive-silence auto-recovery during continuous streaming:
        //    If Gemini transcribed user speech (inputTranscription) and then stayed silent for >1.15s
        //    with zero downlink audio, flush audioStreamEnd and nudge an immediate spoken reply.
        if (!awaiting_response_.load() && !receiving_model_audio_.load() &&
            !speaking_active_.load() && jitter_ring_.Available() == 0 &&
            user_turn_had_speech_.load() && !silent_nudge_used_.load() && !stopping_.load()) {
            int64_t tr_age_us = now_us - last_input_tr_us_.load();
            if (last_input_tr_us_.load() > 0 && tr_age_us > 1150000LL) {
                std::string peek_tr;
                {
                    std::lock_guard<std::mutex> lk(transcript_mutex_);
                    peek_tr = robi::CleanTranscriptText(pending_user_transcript_);
                }
                if (robi::HasMeaningfulSpeechText(peek_tr) && !silent_nudge_used_.exchange(true)) {
                    user_turn_had_speech_.store(false);
                    FlushPendingTranscripts();
                    ESP_LOGW(TAG, "Proactive silence 1.15s after '%s' — forcing spoken reply",
                             peek_tr.c_str());
                    EnqueueAudioStreamEnd();
                    EnqueueSilentTurnNudge(peek_tr);
                } else {
                    user_turn_had_speech_.store(false);
                }
            }
        }

        // 2. Single retry nudge + timeout recovery for explicit clientContent / tool / manual turns
        if (awaiting_response_.load() && !receiving_model_audio_.load() &&
            !speaking_active_.load() && jitter_ring_.Available() == 0 && !stopping_.load()) {
            int64_t waited_us = now_us - awaiting_since_us_.load();
            if (waited_us > 3000000LL && awaiting_nudges_.load() == 0 &&
                tool_hold_until_us_.load() == 0) {
                std::string retry_text;
                {
                    std::lock_guard<std::mutex> clk(client_turn_mutex_);
                    retry_text = last_client_turn_text_;
                }
                if (!retry_text.empty() && awaiting_nudges_.exchange(1) == 0) {
                    ESP_LOGW(TAG, "No audio 3.0s after clientContent — sending single retry nudge");
                    awaiting_since_us_.store(now_us);
                    std::string payload = BuildClientTurnJson(retry_text);
                    if (!payload.empty()) {
                        EnqueueTxJson(std::move(payload));
                    }
                    continue;
                }
            }
            if (waited_us > 5200000LL) {
                int timeouts = consecutive_timeouts_.fetch_add(1) + 1;
                ESP_LOGW(TAG, "Awaiting response timed out after %.1fs (consecutive=%d)",
                         static_cast<double>(waited_us) / 1000000.0, timeouts);
                awaiting_response_.store(false);
                awaiting_nudges_.store(0);
                user_turn_had_speech_.store(false);
                tool_hold_until_us_.store(0);
                if (timeouts >= 2) {
                    HandleFatalSocketError("consecutive_response_timeouts");
                    break;
                }
                if (Application::GetInstance().GetDeviceState() == kDeviceStateListening) {
                    EmitEmotion("listening");
                }
            }
        }

        // 3. Periodic 90s idle farewell check while channel is sitting quiet in Listening
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
            consecutive_timeouts_.store(0);
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
            consecutive_timeouts_.store(0);
            HandleToolCalls(tool_call);
        }

        const cJSON* sc = cJSON_GetObjectItemCaseSensitive(msg, "serverContent");
        if (!cJSON_IsObject(sc)) {
            cJSON_Delete(msg);
            continue;
        }
        consecutive_timeouts_.store(0);

        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(sc, "interrupted"))) {
            if (listening_mode_ != kListeningModeRealtime) {
                ESP_LOGI(TAG, "Ignoring server-side interrupted flag in turn-based mode");
                cJSON_Delete(msg);
                continue;
            }
            turn_gen_.fetch_add(1);
            drop_turn_.store(false);
            turn_complete_.store(false);
            ClearLiveVoiceLevel();
            jitter_ring_.Clear();
            jitter_ring_.NotifyAll();
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
            if (cJSON_IsString(t) && t->valuestring != nullptr && t->valuestring[0] != '\0') {
                int64_t now_tr_us = esp_timer_get_time();
                {
                    std::lock_guard<std::mutex> lk(transcript_mutex_);
                    if (pending_user_transcript_.empty()) {
                        silent_nudge_used_.store(false);
                    }
                    pending_user_transcript_ += t->valuestring;
                }
                last_input_tr_us_.store(now_tr_us);
                last_activity_us_.store(now_tr_us);
                user_turn_had_speech_.store(true);
            }
        }

        // Process modelTurn audio BEFORE outputTranscription on the same message only if needed,
        // or tag emotions at current ring write cursor BEFORE pushing new audio in the same packet
        // so an opening emoji triggers on sample 0 of that clause's audio!
        const cJSON* out_tr = cJSON_GetObjectItemCaseSensitive(sc, "outputTranscription");
        if (cJSON_IsObject(out_tr) && !drop_turn_.load()) {
            const cJSON* t = cJSON_GetObjectItemCaseSensitive(out_tr, "text");
            if (cJSON_IsString(t) && t->valuestring != nullptr) {
                std::string chunk(t->valuestring);
                {
                    std::lock_guard<std::mutex> lk(transcript_mutex_);
                    pending_bot_transcript_ += chunk;
                }
                const char* emo = robi::MatchLeadingOrInlineEmoji(chunk);
                if (emo != nullptr) {
                    clause_had_emoji = true;
                    if (last_emo != emo) {
                        last_emo = emo;
                        jitter_ring_.QueueEmotionAtWriteCursor(emo);
                    }
                }
                clause_buf += chunk;
                if (chunk.find('.') != std::string::npos || chunk.find('!') != std::string::npos ||
                    chunk.find('?') != std::string::npos || chunk.find('\n') != std::string::npos) {
                    if (!clause_had_emoji) {
                        const char* inf = robi::InferHebrewClauseEmotion(clause_buf);
                        if (inf != nullptr && last_emo != inf) {
                            last_emo = inf;
                            jitter_ring_.QueueEmotionAtWriteCursor(inf);
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
                        receiving_model_audio_.store(true);
                        {
                            std::lock_guard<std::mutex> ulk(uplink_mutex_);
                            tx_pcm_batch_.clear();
                            speech_tracker_.Reset();
                        }
                        tool_hold_until_us_.store(0);
                        tool_sent_at_us_.store(0);
                        user_turn_had_speech_.store(false);
                        turn_complete_.store(false);
                        size_t rx_samples = decoded_len / sizeof(int16_t);
                        turn_downlink_samples_.fetch_add(static_cast<uint32_t>(rx_samples));
                        jitter_ring_.PushSamples(reinterpret_cast<const int16_t*>(pcm_bytes.data()),
                                                 rx_samples, stopping_, drop_turn_);
                    }
                }
            }
        }

        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(sc, "turnComplete"))) {
            clause_buf.clear();
            clause_had_emoji = false;
            last_emo.clear();

            int64_t t_sent = tool_sent_at_us_.load();
            if (tool_hold_until_us_.load() > 0 && t_sent > 0 &&
                (esp_timer_get_time() - t_sent) > 30000LL) {
                tool_hold_until_us_.store(0);
            }

            std::string peek_user_tr;
            {
                std::lock_guard<std::mutex> lk(transcript_mutex_);
                peek_user_tr = robi::CleanTranscriptText(pending_user_transcript_);
            }
            FlushPendingTranscripts();

            if (drop_turn_.exchange(false)) {
                jitter_ring_.Clear();
                receiving_model_audio_.store(false);
                user_turn_had_speech_.store(false);
                turn_complete_.store(false);
            } else if (user_turn_had_speech_.exchange(false) &&
                       turn_downlink_samples_.load() == 0 &&
                       jitter_ring_.Available() == 0 &&
                       !speaking_active_.load() &&
                       tool_hold_until_us_.load() == 0 &&
                       robi::HasMeaningfulSpeechText(peek_user_tr) &&
                       !silent_nudge_used_.exchange(true)) {
                ESP_LOGW(TAG, "Silent turnComplete after user said '%s' — nudging spoken reply",
                         peek_user_tr.c_str());
                EnqueueAudioStreamEnd();
                EnqueueSilentTurnNudge(peek_user_tr);
            } else {
                turn_complete_.store(true);
                jitter_ring_.NotifyAll();
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

    while (!stopping_.load()) {
        size_t min_needed = speaking_active
                                ? (rebuffer_needed ? kRebufferSamples : kPlaybackFrameSamples)
                                : kPreBufferSamples;
        jitter_ring_.WaitFor(std::chrono::milliseconds(30),
                             [this, min_needed, speaking_active](size_t ring_count) {
                                 if (stopping_.load() || !ws_connected_.load()) return true;
                                 if (drop_turn_.load()) return true;
                                 if (turn_complete_.load() && (speaking_active || ring_count > 0)) return true;
                                 if (ring_count > 0 &&
                                     (esp_timer_get_time() - last_rx_audio_us_.load()) > 260000LL) {
                                     return true;
                                 }
                                 return ring_count >= min_needed;
                             });

        if (stopping_.load() || !ws_connected_.load()) break;

        if (drop_turn_.exchange(false)) {
            receiving_model_audio_.store(false);
            ClearLiveVoiceLevel();
            if (speaking_active) {
                EmitEmotion("v:0");
                EmitTtsState("stop");
                speaking_active = false;
                speaking_active_.store(false);
            }
            rebuffer_needed = false;
            continue;
        }

        size_t avail = jitter_ring_.Available();
        int64_t idle_rx_us = esp_timer_get_time() - last_rx_audio_us_.load();
        if (!speaking_active) {
            if (avail >= kPreBufferSamples ||
                ((turn_complete_.load() || idle_rx_us > 260000LL) && avail > 0)) {
                speaking_active = true;
                speaking_active_.store(true);
                rebuffer_needed = false;
                initial_burst_left = 3;  // Preload 150ms (3 * 50ms) into AudioService DMA queue
                active_gen = turn_gen_.load();
                awaiting_response_.store(false);
                awaiting_nudges_.store(0);
                {
                    std::lock_guard<std::mutex> clk(client_turn_mutex_);
                    last_client_turn_text_.clear();
                }

                if (Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking) {
                    speaking_audio_ready_.store(true);
                } else {
                    speaking_audio_ready_.store(false);
                    EmitTtsState("start");
                    // Deterministic race-free wait until HandleStateChangedEvent finishes ResetDecoder()
                    for (int wait_spk = 0; wait_spk < 45; ++wait_spk) {
                        if (stopping_.load() || active_gen != turn_gen_.load() ||
                            drop_turn_.load() || speaking_audio_ready_.load(std::memory_order_acquire)) {
                            break;
                        }
                        jitter_ring_.WaitFor(std::chrono::milliseconds(10), [this, active_gen](size_t) {
                            return stopping_.load() || active_gen != turn_gen_.load() ||
                                   drop_turn_.load() ||
                                   speaking_audio_ready_.load(std::memory_order_acquire);
                        });
                    }
                }
            } else {
                int64_t now_chk_us = esp_timer_get_time();
                int64_t th_until = tool_hold_until_us_.load();
                bool hold_expired = (th_until > 0 && now_chk_us >= th_until);
                if (hold_expired) {
                    tool_hold_until_us_.store(0);
                }
                if (avail == 0 && now_chk_us >= tool_hold_until_us_.load() && turn_complete_.load()) {
                    FlushPendingTranscripts();
                    turn_complete_.store(false);
                    receiving_model_audio_.store(false);
                    awaiting_response_.store(false);
                    awaiting_nudges_.store(0);
                    if (Application::GetInstance().GetDeviceState() == kDeviceStateListening) {
                        EmitEmotion("listening");
                    }
                }
                continue;
            }
        }

        if (active_gen != turn_gen_.load() || drop_turn_.exchange(false)) {
            receiving_model_audio_.store(false);
            ClearLiveVoiceLevel();
            EmitEmotion("v:0");
            EmitTtsState("stop");
            speaking_active = false;
            speaking_active_.store(false);
            rebuffer_needed = false;
            continue;
        }

        // Emit any facial emotion whose exact 24kHz audio sample offset has now reached the speaker
        {
            bool flush_remaining = turn_complete_.load() && jitter_ring_.Available() == 0;
            std::string due_emo;
            std::string latest_emo;
            while (jitter_ring_.PopDueEmotion(flush_remaining, due_emo)) {
                if (!due_emo.empty()) {
                    latest_emo = std::move(due_emo);
                }
            }
            if (!latest_emo.empty()) {
                EmitEmotion(latest_emo);
            }
        }

        avail = jitter_ring_.Available();
        idle_rx_us = esp_timer_get_time() - last_rx_audio_us_.load();
        size_t pop_threshold = rebuffer_needed ? kRebufferSamples : kPlaybackFrameSamples;
        if (avail >= pop_threshold ||
            ((turn_complete_.load() || idle_rx_us > 260000LL) && avail > 0)) {
            if (rebuffer_needed) {
                rebuffer_needed = false;
                initial_burst_left = 3;
            }
            size_t to_pop = std::min(kPlaybackFrameSamples, avail);
            size_t got = jitter_ring_.PopSamples(frame_buf.data(), to_pop);
            jitter_ring_.NotifyOne();

            if (got > 0) {
                audio_frames_played_.fetch_add(1);
                int lvl = robi::ComputeVoiceLevel0To100(frame_buf.data(), got);
                live_voice_level_.store(lvl, std::memory_order_relaxed);
                live_voice_updated_us_.store(esp_timer_get_time(), std::memory_order_relaxed);

                auto pkt = std::make_unique<AudioStreamPacket>();
                pkt->sample_rate = 24000;
                pkt->frame_duration = 0;  // 0 = raw 24kHz int16 PCM bypass in AudioService
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
                    pkt = std::make_unique<AudioStreamPacket>();
                    pkt->sample_rate = 24000;
                    pkt->frame_duration = 0;
                    pkt->payload.resize(got * sizeof(int16_t));
                    std::memcpy(pkt->payload.data(), frame_buf.data(), pkt->payload.size());
                    vTaskDelay(pdMS_TO_TICKS(16));
                }

                if (initial_burst_left > 0) {
                    --initial_burst_left;
                } else {
                    // Feed 50ms frames every 46ms so AudioService stays gently cushioned without draining PSRAM ring
                    vTaskDelay(pdMS_TO_TICKS(46));
                }
            }
            continue;
        }

        // Ring has fewer than kPlaybackFrameSamples (or 0 when drained)
        if (!turn_complete_.load()) {
            auto& as = Application::GetInstance().GetAudioService();
            if (as.IsPlaybackIdle()) {
                if (esp_timer_get_time() < tool_hold_until_us_.load()) {
                    rebuffer_needed = true;
                    continue;
                }
                if (avail == 0 && idle_rx_us > 1500000LL) {
                    ESP_LOGW(TAG, "Synthesizing turnComplete after 1.5s audio stream idle");
                    turn_complete_.store(true);
                } else {
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
               !as.IsPlaybackIdle() && wait_loops < 250) {
            if (jitter_ring_.Available() > 0 || esp_timer_get_time() < tool_hold_until_us_.load()) {
                more_audio_arrived = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            ++wait_loops;
        }
        if (more_audio_arrived) {
            turn_complete_.store(false);
            continue;
        }

        FlushPendingTranscripts();
        turn_complete_.store(false);
        receiving_model_audio_.store(false);
        speaking_active = false;
        speaking_active_.store(false);
        speaking_audio_ready_.store(false);
        rebuffer_needed = false;
        ClearLiveVoiceLevel();

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
            awaiting_nudges_.store(0);
            int64_t end_us = esp_timer_get_time();
            post_playback_guard_until_us_.store(end_us + kPostPlaybackGuardUs);
            EmitEmotion("v:0");
            last_activity_us_.store(end_us);
            EmitTtsState("stop");

            if (close_after_turn_.exchange(false)) {
                vTaskDelay(pdMS_TO_TICKS(220));
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
        } else if (name == "save_memory") {
            if (!speaking_active_.load() && turn_downlink_samples_.load() == 0) {
                expect_continuation = true;
            }
            const cJSON* n = args ? cJSON_GetObjectItemCaseSensitive(args, "note") : nullptr;
            if (cJSON_IsString(n) && n->valuestring != nullptr) {
                RobiConfig::GetInstance().AddMemoryNote(n->valuestring);
                cJSON_AddStringToObject(res_obj, "result", "saved");
            } else {
                cJSON_AddStringToObject(res_obj, "result", "ignored");
            }
        } else {
            expect_continuation = true;
            if (name == "set_volume") {
                int vol = 70;
                const cJSON* v = args ? cJSON_GetObjectItemCaseSensitive(args, "volume") : nullptr;
                if (cJSON_IsNumber(v)) vol = std::clamp(v->valueint, 0, 100);
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
        tool_sent_at_us_.store(now_us);
        // Hold up to 2.8s for post-tool spoken continuation (released early on first audio or post-tool turnComplete)
        tool_hold_until_us_.store(now_us + 2800000LL);
        awaiting_response_.store(true);
        awaiting_since_us_.store(now_us);
        awaiting_nudges_.store(0);
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
