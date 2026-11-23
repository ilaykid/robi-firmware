#include "robi_http_portal.h"
#include "robi_config.h"
#include "robi_gemini_live.h"
#include "robi_imu.h"
#include "robi_mdns.h"
#include "robi_portal_html.h"
#include "robi_sfx.h"

#include "application.h"
#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "display.h"
#include "wifi_board.h"

#include <arpa/inet.h>
#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <netinet/in.h>
#include <ssid_manager.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cstring>
#include <string>

#define TAG "RobiPortal"

namespace {

esp_err_t HandleIndexGet(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, robi::kPortalHtml, sizeof(robi::kPortalHtml) - 1);
}

esp_err_t HandleFaviconGet(httpd_req_t* req) {
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t HandleConfigGet(httpd_req_t* req) {
    std::string json = RobiConfig::GetInstance().ExportJson(/*mask_api_key=*/true);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, json.data(), json.size());
}

esp_err_t ReadBody(httpd_req_t* req, std::string& out, size_t max_len) {
    if (req->content_len == 0 || req->content_len > max_len) {
        return ESP_FAIL;
    }
    out.resize(req->content_len);
    size_t received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, out.data() + received, req->content_len - received);
        if (r <= 0) return ESP_FAIL;
        received += static_cast<size_t>(r);
    }
    return ESP_OK;
}

bool VerifyCsrfHeader(httpd_req_t* req) {
    char tok[16] = {};
    if (httpd_req_get_hdr_value_str(req, "X-Robi-Token", tok, sizeof(tok)) != ESP_OK ||
        std::strcmp(tok, "1") != 0) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Missing CSRF token");
        return false;
    }
    char origin[128] = {};
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK && origin[0] != '\0') {
        bool allowed = (std::strncmp(origin, "http://robi.local", 17) == 0) ||
                       (std::strncmp(origin, "http://robi", 11) == 0) ||
                       (std::strncmp(origin, "http://192.168.", 15) == 0) ||
                       (std::strncmp(origin, "http://10.", 10) == 0) ||
                       (std::strncmp(origin, "http://172.", 11) == 0);
        if (!allowed) {
            httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Origin not allowed");
            return false;
        }
    }
    return true;
}

esp_err_t HandleConfigPost(httpd_req_t* req) {
    if (!VerifyCsrfHeader(req)) {
        return ESP_FAIL;
    }
    std::string body;
    if (ReadBody(req, body, RobiConfig::kMaxConfigJsonBytes) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    std::string err;
    bool session_changed = false;
    if (!RobiConfig::GetInstance().UpdateFromJson(body, false, err, &session_changed)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err.c_str());
        return ESP_FAIL;
    }

    GeminiLiveProtocol::ResetModelCooldowns();

    // Apply updated voice/model/language/prompt immediately on idle or quiet listening sessions
    Application::GetInstance().Schedule([session_changed]() {
        auto& app = Application::GetInstance();
        auto dev_state = app.GetDeviceState();
        if (dev_state == kDeviceStateIdle) {
            app.DismissAlert();
            if (session_changed) {
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    if (proto->IsAudioChannelOpened()) {
                        proto->CloseAudioChannel(false);
                    }
                }
            }
        } else if (session_changed && dev_state == kDeviceStateListening) {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                if (proto->IsAudioChannelOpened() && !proto->IsAwaitingResponse()) {
                    proto->CloseAudioChannel(false);
                }
            }
        }
    });

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

esp_err_t HandleStatusGet(httpd_req_t* req) {
    bool include_ui = false;
    char query_buf[64] = {};
    if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
        char day_buf[16] = {};
        if (httpd_query_key_value(query_buf, "day", day_buf, sizeof(day_buf)) == ESP_OK) {
            RobiConfig::GetInstance().SyncUsageDay(day_buf);
        }
        char ui_buf[8] = {};
        if (httpd_query_key_value(query_buf, "ui", ui_buf, sizeof(ui_buf)) == ESP_OK &&
            ui_buf[0] == '1') {
            include_ui = true;
        }
    }

    auto& board = Board::GetInstance();
    auto& wifi = WifiManager::GetInstance();
    auto& app = Application::GetInstance();
    auto cfg = RobiConfig::GetInstance().Get();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", wifi.GetSsid().c_str());
    cJSON_AddStringToObject(root, "ip", wifi.GetIpAddress().c_str());
    cJSON_AddNumberToObject(root, "rssi", wifi.GetRssi());
    cJSON* saved_arr = cJSON_CreateArray();
    for (const auto& item : SsidManager::GetInstance().GetSsidList()) {
        cJSON_AddItemToArray(saved_arr, cJSON_CreateString(item.ssid.c_str()));
    }
    cJSON_AddItemToObject(root, "saved_ssids", saved_arr);
    cJSON_AddStringToObject(root, "preferred_model", cfg.preferred_model.c_str());
    cJSON_AddStringToObject(root, "persona_mode", cfg.persona_mode.c_str());
    cJSON_AddStringToObject(root, "active_scope", RobiConfig::GetInstance().GetActiveScope().c_str());
    cJSON_AddStringToObject(root, "usage_day", RobiConfig::GetInstance().GetUsageDay().c_str());

    const char* st_str = "unknown";
    switch (app.GetDeviceState()) {
        case kDeviceStateIdle: st_str = "idle"; break;
        case kDeviceStateConnecting: st_str = "connecting"; break;
        case kDeviceStateListening: st_str = "listening"; break;
        case kDeviceStateSpeaking: st_str = "speaking"; break;
        case kDeviceStateWifiConfiguring: st_str = "wifi_config"; break;
        case kDeviceStateUpgrading: st_str = "upgrading"; break;
        case kDeviceStateActivating: st_str = "activating"; break;
        default: break;
    }
    cJSON_AddStringToObject(root, "state", st_str);
    cJSON_AddNumberToObject(root, "free_sram",
                            static_cast<double>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    cJSON_AddNumberToObject(root, "min_sram",
                            static_cast<double>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    cJSON_AddNumberToObject(
        root, "largest_dma_sram",
        static_cast<double>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)));
    cJSON_AddNumberToObject(root, "free_psram",
                            static_cast<double>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    std::string active_model;
    std::string last_model = GeminiLiveProtocol::GetLastConnectedModel();
    if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
        bool open = proto->IsAudioChannelOpened();
        cJSON_AddBoolToObject(root, "channel_open", open);
        cJSON_AddNumberToObject(root, "turns_completed", proto->GetTurnsCompleted());
        cJSON_AddNumberToObject(root, "audio_frames_played", proto->GetAudioFramesPlayed());
        cJSON_AddNumberToObject(root, "tx_audio_packets", proto->GetTxAudioPackets());
        cJSON_AddNumberToObject(root, "rx_ws_msgs", proto->GetRxWsMessages());
        cJSON_AddNumberToObject(root, "mic_rms", proto->GetLastMicRms());
        cJSON_AddNumberToObject(root, "peak_mic_rms", proto->GetPeakMicRms());
        cJSON_AddNumberToObject(root, "noise_floor_rms", proto->GetNoiseFloorRms());
        cJSON_AddNumberToObject(root, "ring_avail", static_cast<double>(proto->GetRingAvailableSamples()));
        cJSON_AddBoolToObject(root, "playback_idle", app.GetAudioService().IsPlaybackIdle());
        cJSON_AddNumberToObject(root, "playback_q_depth",
                                static_cast<double>(app.GetAudioService().GetPlaybackQueueDepth()));
        cJSON_AddBoolToObject(root, "awaiting_response", proto->IsAwaitingResponse());
        cJSON_AddBoolToObject(root, "streaming_speech", proto->IsStreamingSpeech());
        if (open) {
            active_model = proto->GetActiveModel();
        }
    }
    cJSON_AddStringToObject(root, "active_model", active_model.c_str());
    cJSON_AddStringToObject(root, "last_model", last_model.c_str());

    static const char* const kModelIds[3] = {
        "gemini-3.8-live",
        "gemini-2.5-flash-native-audio-preview-12-2025",
        "gemini-2.0-flash-live-001",
    };
    auto usage_vec = RobiConfig::GetInstance().GetModelUsage();
    cJSON* models_arr = cJSON_CreateArray();
    uint32_t tot_sessions = 0, tot_turns = 0, tot_audio_sec = 0, tot_tokens = 0;
    for (int i = 0; i < 3; ++i) {
        const char* mid = kModelIds[i];
        uint32_t s = 0, t = 0, a = 0, tok = 0;
        for (const auto& u : usage_vec) {
            if (u.id == mid) {
                s = u.sessions;
                t = u.turns;
                a = u.audio_sec;
                tok = u.tokens;
                break;
            }
        }
        tot_sessions += s;
        tot_turns += t;
        tot_audio_sec += a;
        tot_tokens += tok;

        cJSON* m_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(m_obj, "id", mid);
        cJSON_AddNumberToObject(m_obj, "sessions", s);
        cJSON_AddNumberToObject(m_obj, "turns", t);
        cJSON_AddNumberToObject(m_obj, "audio_sec", a);
        cJSON_AddNumberToObject(m_obj, "tokens", tok);
        cJSON_AddNumberToObject(m_obj, "cooldown_sec", GeminiLiveProtocol::GetModelCooldownSec(i));
        cJSON_AddBoolToObject(m_obj, "active", active_model == mid);
        cJSON_AddBoolToObject(m_obj, "last_used", last_model == mid);
        cJSON_AddItemToArray(models_arr, m_obj);
    }
    cJSON_AddItemToObject(root, "models", models_arr);

    cJSON* quota_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(quota_obj, "used_sessions", tot_sessions);
    cJSON_AddNumberToObject(quota_obj, "max_sessions", 500);
    cJSON_AddNumberToObject(quota_obj, "used_turns", tot_turns);
    cJSON_AddNumberToObject(quota_obj, "used_audio_sec", tot_audio_sec);
    cJSON_AddNumberToObject(quota_obj, "max_audio_sec", 2700);
    cJSON_AddNumberToObject(quota_obj, "used_tokens", tot_tokens);
    cJSON_AddNumberToObject(quota_obj, "max_tokens", 1000000);
    cJSON_AddItemToObject(root, "daily_quota", quota_obj);

    int bat = 100;
    bool chg = false, dis = false;
    if (board.GetBatteryLevel(bat, chg, dis)) {
        cJSON_AddNumberToObject(root, "battery", bat);
        cJSON_AddBoolToObject(root, "charging", chg);
    }
    if (auto* codec = board.GetAudioCodec()) {
        cJSON_AddNumberToObject(root, "volume", codec->output_volume());
    }
    if (auto* bl = board.GetBacklight()) {
        cJSON_AddNumberToObject(root, "brightness", bl->brightness());
    }

    cJSON* mems = cJSON_CreateArray();
    for (const auto& m : RobiConfig::GetInstance().GetMemoryNotes()) {
        cJSON_AddItemToArray(mems, cJSON_CreateString(m.c_str()));
    }
    cJSON_AddItemToObject(root, "memories", mems);

    auto& imu = robi::RobiImuService::GetInstance();
    cJSON* imu_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(imu_obj, "ok", imu.IsAvailable());
    cJSON_AddNumberToObject(imu_obj, "addr", imu.GetAddress());
    cJSON_AddBoolToObject(imu_obj, "face_down", imu.IsFaceDown());
    cJSON_AddNumberToObject(imu_obj, "z_sign", imu.GetZFrontSign());
    auto samp = imu.GetLastSample();
    cJSON_AddNumberToObject(imu_obj, "ax", std::round(samp.ax * 100.0f) / 100.0f);
    cJSON_AddNumberToObject(imu_obj, "ay", std::round(samp.ay * 100.0f) / 100.0f);
    cJSON_AddNumberToObject(imu_obj, "az", std::round(samp.az * 100.0f) / 100.0f);
    cJSON_AddItemToObject(root, "imu", imu_obj);

    if (include_ui) {
        std::string ui_raw = RobiConfig::GetInstance().GetUiStatusJson();
        cJSON* ui_obj = cJSON_ParseWithLength(ui_raw.data(), ui_raw.size());
        if (ui_obj != nullptr) {
            cJSON_AddItemToObject(root, "ui", ui_obj);
        }
    }

    char* s = cJSON_PrintUnformatted(root);
    std::string out = s ? s : "{}";
    if (s) cJSON_free(s);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, out.data(), out.size());
}

esp_err_t HandleMemoryClearPost(httpd_req_t* req) {
    if (!VerifyCsrfHeader(req)) {
        return ESP_FAIL;
    }
    RobiConfig::GetInstance().ClearMemoryNotes();
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

esp_err_t HandleControlPost(httpd_req_t* req) {
    if (!VerifyCsrfHeader(req)) {
        return ESP_FAIL;
    }
    std::string body;
    if (ReadBody(req, body, 1024) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_FAIL;
    }
    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    // Dispatch hardware I2C/NVS volume & brightness updates onto Internal SRAM main task
    const cJSON* vol = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (cJSON_IsNumber(vol)) {
        int v = std::clamp(vol->valueint, 0, 100);
        Application::GetInstance().Schedule([v]() {
            if (auto* codec = Board::GetInstance().GetAudioCodec()) {
                codec->SetOutputVolume(v);
            }
        });
    }
    const cJSON* bri = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (cJSON_IsNumber(bri)) {
        int b = std::clamp(bri->valueint, 10, 100);
        Application::GetInstance().Schedule([b]() {
            if (auto* bl = Board::GetInstance().GetBacklight()) {
                bl->SetBrightness(static_cast<uint8_t>(b), true);
            }
        });
    }
    const cJSON* emo = cJSON_GetObjectItemCaseSensitive(root, "emotion");
    if (cJSON_IsString(emo) && emo->valuestring != nullptr) {
        std::string e_str = emo->valuestring;
        Application::GetInstance().Schedule([e_str]() {
            if (auto* disp = Board::GetInstance().GetDisplay()) {
                disp->SetEmotion(e_str.c_str());
            }
        });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "toggle_chat"))) {
        Application::GetInstance().Schedule([]() { Application::GetInstance().ToggleChatState(); });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "commit_utterance"))) {
        Application::GetInstance().Schedule([]() {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                proto->CommitListeningUtterance();
            }
        });
    }
    const cJSON* p_scope = cJSON_GetObjectItemCaseSensitive(root, "persona_scope");
    if (cJSON_IsString(p_scope) && p_scope->valuestring != nullptr && p_scope->valuestring[0] != '\0') {
        RobiConfig::GetInstance().SetActiveScope(p_scope->valuestring);
    }
    const cJSON* z_sign = cJSON_GetObjectItemCaseSensitive(root, "imu_z_sign");
    if (cJSON_IsNumber(z_sign)) {
        robi::RobiImuService::GetInstance().OverrideZFrontSign(static_cast<float>(z_sign->valuedouble));
    }
    const cJSON* sfx = cJSON_GetObjectItemCaseSensitive(root, "play_sfx");
    if (cJSON_IsNumber(sfx)) {
        int s_id = std::clamp(sfx->valueint, 0, 7);
        Application::GetInstance().Schedule([s_id]() {
            robi::PlayRobiSfx(static_cast<robi::RobiSfxId>(s_id), false);
        });
    }
    const cJSON* prompt = cJSON_GetObjectItemCaseSensitive(root, "prompt");
    if (cJSON_IsString(prompt) && prompt->valuestring != nullptr && prompt->valuestring[0] != '\0') {
        std::string p_str = prompt->valuestring;
        if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
            proto->InjectUserPromptTurn(p_str);
        }
    }
    const cJSON* sim_mic = cJSON_GetObjectItemCaseSensitive(root, "simulate_mic_ms");
    if (cJSON_IsNumber(sim_mic) && sim_mic->valueint > 0) {
        int dur = sim_mic->valueint;
        Application::GetInstance().Schedule([dur]() {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                proto->SimulateMicUtterance(dur);
            }
        });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "close_channel"))) {
        Application::GetInstance().Schedule([]() {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                if (proto->IsAudioChannelOpened()) {
                    proto->CloseAudioChannel(false);
                }
            }
        });
    }
    const cJSON* add_ssid = cJSON_GetObjectItemCaseSensitive(root, "add_wifi_ssid");
    const cJSON* add_pass = cJSON_GetObjectItemCaseSensitive(root, "add_wifi_pass");
    if (cJSON_IsString(add_ssid) && add_ssid->valuestring != nullptr && add_ssid->valuestring[0] != '\0') {
        std::string ws = add_ssid->valuestring;
        std::string wp = (cJSON_IsString(add_pass) && add_pass->valuestring != nullptr)
                             ? add_pass->valuestring
                             : "";
        SsidManager::GetInstance().AddSsid(ws, wp);
    }
    const cJSON* rm_ssid = cJSON_GetObjectItemCaseSensitive(root, "remove_wifi_ssid");
    if (cJSON_IsString(rm_ssid) && rm_ssid->valuestring != nullptr && rm_ssid->valuestring[0] != '\0') {
        std::string target = rm_ssid->valuestring;
        const auto& list = SsidManager::GetInstance().GetSsidList();
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].ssid == target) {
                SsidManager::GetInstance().RemoveSsid(static_cast<int>(i));
                break;
            }
        }
    }
    const cJSON* conn_ssid = cJSON_GetObjectItemCaseSensitive(root, "connect_wifi_ssid");
    if (cJSON_IsString(conn_ssid) && conn_ssid->valuestring != nullptr && conn_ssid->valuestring[0] != '\0') {
        std::string target = conn_ssid->valuestring;
        Application::GetInstance().Schedule([target]() {
            const auto& list = SsidManager::GetInstance().GetSsidList();
            for (size_t i = 0; i < list.size(); ++i) {
                if (list[i].ssid == target) {
                    std::string pass = list[i].password;
                    SsidManager::GetInstance().SetDefaultSsid(static_cast<int>(i));
                    wifi_config_t wcfg = {};
                    if (esp_wifi_get_config(WIFI_IF_STA, &wcfg) == ESP_OK) {
                        std::memset(wcfg.sta.ssid, 0, sizeof(wcfg.sta.ssid));
                        std::memset(wcfg.sta.password, 0, sizeof(wcfg.sta.password));
                        std::memcpy(wcfg.sta.ssid, target.data(),
                                    std::min(target.size(), sizeof(wcfg.sta.ssid)));
                        std::memcpy(wcfg.sta.password, pass.data(),
                                    std::min(pass.size(), sizeof(wcfg.sta.password)));
                        wcfg.sta.bssid_set = 0;
                        wcfg.sta.channel = 0;
                        esp_wifi_set_config(WIFI_IF_STA, &wcfg);
                    }
                    esp_wifi_disconnect();
                    break;
                }
            }
        });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "reboot"))) {
        Application::GetInstance().Schedule([]() { Application::GetInstance().Reboot(); });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "reset_usage"))) {
        RobiConfig::GetInstance().ResetDailyUsage();
        GeminiLiveProtocol::ResetModelCooldowns();
    }
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

}  // namespace

RobiHttpPortal& RobiHttpPortal::GetInstance() {
    static RobiHttpPortal instance;
    return instance;
}

void RobiHttpPortal::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (server_ != nullptr) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;
    config.max_open_sockets = 4;
    config.backlog_conn = 4;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) == ESP_OK) {
        static const httpd_uri_t kUris[] = {
            {"/", HTTP_GET, HandleIndexGet, nullptr},
            {"/favicon.ico", HTTP_GET, HandleFaviconGet, nullptr},
            {"/api/config", HTTP_GET, HandleConfigGet, nullptr},
            {"/api/config", HTTP_POST, HandleConfigPost, nullptr},
            {"/api/status", HTTP_GET, HandleStatusGet, nullptr},
            {"/api/memory/clear", HTTP_POST, HandleMemoryClearPost, nullptr},
            {"/api/control", HTTP_POST, HandleControlPost, nullptr},
        };
        for (const auto& u : kUris) {
            httpd_register_uri_handler(server_, &u);
        }
        std::string ip = WifiManager::GetInstance().GetIpAddress();
        ESP_LOGI(TAG, "Robi Parent Portal active at http://robi.local and http://%s", ip.c_str());
    } else {
        ESP_LOGW(TAG, "Failed to start HTTP server on port 80");
        server_ = nullptr;
    }

    if (!mdns_running_.load()) {
        mdns_running_.store(true);
        if (mdns_stack_ == nullptr) {
            mdns_stack_ = static_cast<StackType_t*>(
                heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        }
        if (mdns_tcb_ == nullptr) {
            mdns_tcb_ = static_cast<StaticTask_t*>(
                heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }
        if (mdns_stack_ != nullptr && mdns_tcb_ != nullptr) {
            mdns_task_ = xTaskCreateStatic(
                [](void* arg) {
                    static_cast<RobiHttpPortal*>(arg)->MdnsResponderLoop();
                    vTaskDelete(nullptr);
                },
                "robi_mdns", 4096, this, 2, mdns_stack_, mdns_tcb_);
        } else {
            xTaskCreate(
                [](void* arg) {
                    static_cast<RobiHttpPortal*>(arg)->MdnsResponderLoop();
                    vTaskDelete(nullptr);
                },
                "robi_mdns", 4096, this, 2, &mdns_task_);
        }
    }
}

void RobiHttpPortal::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    mdns_running_.store(false);
    if (mdns_sock_ >= 0) {
        shutdown(mdns_sock_, SHUT_RDWR);
        close(mdns_sock_);
        mdns_sock_ = -1;
    }
    mdns_task_ = nullptr;

    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
        ESP_LOGI(TAG, "Robi Parent Portal stopped");
    }
}

void RobiHttpPortal::MdnsResponderLoop() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        mdns_running_.store(false);
        return;
    }
    mdns_sock_ = sock;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int ttl255 = 255;
    uint8_t mcast_ttl255 = 255;
    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl255, sizeof(ttl255));
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &mcast_ttl255, sizeof(mcast_ttl255));

    sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(5353);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        close(sock);
        mdns_sock_ = -1;
        mdns_running_.store(false);
        return;
    }

    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    timeval tv = {};
    tv.tv_sec = 2;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in mcast_dst = {};
    mcast_dst.sin_family = AF_INET;
    mcast_dst.sin_port = htons(5353);
    mcast_dst.sin_addr.s_addr = inet_addr("224.0.0.251");

    uint32_t joined_ip = 0;
    int64_t last_announce_us = 0;

    uint8_t buf[256];
    while (mdns_running_.load()) {
        std::string ip_str = WifiManager::GetInstance().GetIpAddress();
        in_addr ip_bin = {};
        bool have_ip = (!ip_str.empty() && inet_aton(ip_str.c_str(), &ip_bin) != 0 && ip_bin.s_addr != 0);

        if (have_ip && ip_bin.s_addr != joined_ip) {
            joined_ip = ip_bin.s_addr;
            ip_mreq mreq_if = {};
            mreq_if.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
            mreq_if.imr_interface.s_addr = ip_bin.s_addr;
            setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq_if, sizeof(mreq_if));
            setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq_if, sizeof(mreq_if));
            setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &ip_bin, sizeof(ip_bin));

            uint8_t ann[64];
            size_t alen = robi::BuildMdnsRobiLocalPacket(
                ann, 0, reinterpret_cast<const uint8_t*>(&ip_bin.s_addr), 1);
            sendto(sock, ann, alen, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
            last_announce_us = esp_timer_get_time();
        }

        int64_t now_us = esp_timer_get_time();
        if (have_ip && (now_us - last_announce_us) > 10LL * 1000000LL) {
            uint8_t ann[64];
            size_t alen = robi::BuildMdnsRobiLocalPacket(
                ann, 0, reinterpret_cast<const uint8_t*>(&ip_bin.s_addr), 1);
            sendto(sock, ann, alen, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
            last_announce_us = now_us;
        }

        sockaddr_in src = {};
        socklen_t slen = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &slen);
        if (n <= 0 || !have_ip) continue;

        uint16_t qtype = 1;
        if (!robi::ParseMdnsRobiLocalQuery(buf, static_cast<size_t>(n), qtype)) {
            continue;
        }

        uint16_t txid = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
        uint8_t resp[64];
        size_t off = robi::BuildMdnsRobiLocalPacket(
            resp, 0, reinterpret_cast<const uint8_t*>(&ip_bin.s_addr), qtype);
        sendto(sock, resp, off, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));

        resp[0] = static_cast<uint8_t>((txid >> 8) & 0xFFu);
        resp[1] = static_cast<uint8_t>(txid & 0xFFu);
        sendto(sock, resp, off, 0, reinterpret_cast<sockaddr*>(&src), slen);
    }

    if (mdns_sock_ >= 0) {
        close(mdns_sock_);
        mdns_sock_ = -1;
    }
}
