#include "robi_config.h"

#include <cJSON.h>
#include <esp_event.h>
#include <esp_flash.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ssid_manager.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#define TAG "RobiConfig"

namespace {

constexpr const char* kNvsNsCfg = "robi_cfg";
constexpr const char* kNvsKeyJson = "json";
constexpr const char* kNvsKeyFlashSig = "flash_sig";
constexpr const char* kNvsKeyGoodBssid = "good_bssid";
constexpr const char* kNvsNsMem = "robi_mem";
constexpr const char* kNvsKeyMemJson = "notes";
constexpr const char* kFlashMagic = "ROBICFG1";

// Smart Multi-AP DHCP Failover Guard:
// If a mesh extender/repeater associates at 802.11 L2 (WIFI_EVENT_STA_CONNECTED) but fails to
// hand out a DHCP IP within 4.5s, blacklist that BSSID and immediately advance WifiStation's
// connect_queue_ to the working router BSSID, and remember the working BSSID in NVS.
bool s_wifi_guard_installed = false;
esp_timer_handle_t s_dhcp_watchdog_timer = nullptr;
std::array<uint8_t, 6> s_cur_bssid = {};
std::array<uint8_t, 6> s_good_bssid = {};
bool s_good_bssid_valid = false;
int s_fast_skip_count = 0;
std::vector<std::array<uint8_t, 6>> s_dead_dhcp_bssids;

bool IsDeadDhcpBssid(const std::array<uint8_t, 6>& b) {
    for (const auto& d : s_dead_dhcp_bssids) {
        if (d == b) return true;
    }
    return false;
}

void InstallSmartWifiDhcpGuard() {
    if (s_wifi_guard_installed) return;
    s_wifi_guard_installed = true;

    // Load last known DHCP-working BSSID from NVS
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsCfg, NVS_READONLY, &h) == ESP_OK) {
        size_t len = 6;
        if (nvs_get_blob(h, kNvsKeyGoodBssid, s_good_bssid.data(), &len) == ESP_OK && len == 6) {
            s_good_bssid_valid = true;
            ESP_LOGI(TAG, "Loaded last good DHCP BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                     s_good_bssid[0], s_good_bssid[1], s_good_bssid[2],
                     s_good_bssid[3], s_good_bssid[4], s_good_bssid[5]);
        }
        nvs_close(h);
    }

    esp_event_loop_create_default();  // Safe no-op if already created

    esp_timer_create_args_t targs = {};
    targs.callback = [](void*) {
        if (!IsDeadDhcpBssid(s_cur_bssid)) {
            ESP_LOGW(TAG, "DHCP timeout (4.5s) on BSSID %02x:%02x:%02x:%02x:%02x:%02x — blacklisting extender & advancing AP queue!",
                     s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                     s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5]);
            s_dead_dhcp_bssids.push_back(s_cur_bssid);
        }
        esp_wifi_disconnect();
    };
    targs.dispatch_method = ESP_TIMER_TASK;
    targs.name = "robi_dhcp_wd";
    esp_timer_create(&targs, &s_dhcp_watchdog_timer);

    esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void*, esp_event_base_t, int32_t event_id, void* event_data) {
            if (event_id == WIFI_EVENT_STA_CONNECTED && event_data != nullptr) {
                auto* ev = static_cast<wifi_event_sta_connected_t*>(event_data);
                std::memcpy(s_cur_bssid.data(), ev->bssid, 6);
                if (s_dhcp_watchdog_timer == nullptr) return;
                esp_timer_stop(s_dhcp_watchdog_timer);

                if (IsDeadDhcpBssid(s_cur_bssid)) {
                    ESP_LOGW(TAG, "Fast-skipping dead DHCP BSSID %02x:%02x:%02x:%02x:%02x:%02x",
                             s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                             s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5]);
                    esp_timer_start_once(s_dhcp_watchdog_timer, 25000ULL);  // 25ms
                    return;
                }
                if (s_good_bssid_valid && s_cur_bssid != s_good_bssid && s_fast_skip_count < 6) {
                    ++s_fast_skip_count;
                    if (s_fast_skip_count >= 6 && !IsDeadDhcpBssid(s_cur_bssid)) {
                        s_dead_dhcp_bssids.push_back(s_cur_bssid);
                    }
                    ESP_LOGI(TAG, "Fast-skipping non-preferred BSSID %02x:%02x:%02x:%02x:%02x:%02x (%d/6) to reach good router",
                             s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                             s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5], s_fast_skip_count);
                    esp_timer_start_once(s_dhcp_watchdog_timer, 25000ULL);  // 25ms
                    return;
                }
                esp_timer_start_once(s_dhcp_watchdog_timer, 4500000ULL);  // 4.5s for DHCP
            } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                if (s_dhcp_watchdog_timer != nullptr) {
                    esp_timer_stop(s_dhcp_watchdog_timer);
                }
            }
        },
        nullptr, nullptr);

    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void*, esp_event_base_t, int32_t, void*) {
            if (s_dhcp_watchdog_timer != nullptr) {
                esp_timer_stop(s_dhcp_watchdog_timer);
            }
            if (!s_good_bssid_valid || s_good_bssid != s_cur_bssid) {
                s_good_bssid = s_cur_bssid;
                s_good_bssid_valid = true;
                nvs_handle_t nh = 0;
                if (nvs_open(kNvsNsCfg, NVS_READWRITE, &nh) == ESP_OK) {
                    nvs_set_blob(nh, kNvsKeyGoodBssid, s_good_bssid.data(), 6);
                    nvs_commit(nh);
                    nvs_close(nh);
                }
                ESP_LOGI(TAG, "Saved verified DHCP BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                         s_good_bssid[0], s_good_bssid[1], s_good_bssid[2],
                         s_good_bssid[3], s_good_bssid[4], s_good_bssid[5]);
            }
        },
        nullptr, nullptr);
}

uint32_t Fnv1aHash(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h == 0 ? 1u : h;
}

std::string SafeGetStr(const cJSON* obj, const char* key, const std::string& def = "") {
    const cJSON* it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(it) && it->valuestring != nullptr) {
        return std::string(it->valuestring);
    }
    return def;
}

int SafeGetInt(const cJSON* obj, const char* key, int def) {
    const cJSON* it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it)) {
        return it->valueint;
    }
    return def;
}

bool IsValidTheme(const std::string& t) {
    return t == "cyber_cyan" || t == "solar_gold" || t == "emerald_matrix" ||
           t == "cosmic_violet" || t == "coral_cute";
}

bool IsValidFaceStyle(const std::string& s) {
    return s == "cyber_visor" || s == "round_kawaii" || s == "retro_pixel" ||
           s == "visor_cyclops" || s == "owl_mech";
}

bool IsValidVoice(const std::string& v) {
    static const char* const kVoices[] = {
        "Puck", "Fenrir", "Zephyr", "Leda", "Aoede", "Kore",
        "Charon", "Achird", "Finn", "Sadachbia", "Orus", "Sulafat",
    };
    for (const char* allowed : kVoices) {
        if (v == allowed) return true;
    }
    return false;
}

std::vector<RobiActivity> DefaultActivitiesForKid(const std::string& language) {
    if (language == "en-US") {
        return {
            {"Math", "Let's practice fun math puzzles together!"},
            {"Science", "Tell me an amazing fact about space, science, or nature!"},
            {"Riddles", "Give me a fun detective riddle!"},
            {"Story", "Let's continue or invent an adventure story together!"},
        };
    }
    return {
        {"חשבון", "בוא נתרגל חשבון בכיף"},
        {"אנגלית", "בוא נתרגל אנגלית בכיף"},
        {"ידע כללי", "ספר לי משהו מרתק ומפתיע במדע, חלל, היסטוריה או טבע!"},
        {"סיפור", "בוא נמשיך או נמציא יחד סיפור הרפתקאות מותח"},
    };
}

}  // namespace

RobiConfig& RobiConfig::GetInstance() {
    static RobiConfig instance;
    return instance;
}

uint32_t RobiConfig::ParseHexColor(const std::string& s, uint32_t fallback) {
    if (s.empty()) return fallback;
    const char* p = s.c_str();
    if (*p == '#') ++p;
    if (std::strlen(p) != 6) return fallback;
    char* end = nullptr;
    unsigned long val = std::strtoul(p, &end, 16);
    if (end == nullptr || *end != '\0') return fallback;
    return static_cast<uint32_t>(val & 0xFFFFFFu);
}

void RobiConfig::EnsureLoaded() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_) return;
    loaded_ = true;

    // 1. Load existing NVS config if present
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsCfg, NVS_READONLY, &h) == ESP_OK) {
        size_t req = 0;
        if (nvs_get_blob(h, kNvsKeyJson, nullptr, &req) == ESP_OK && req > 0 &&
            req <= kMaxConfigJsonBytes) {
            std::string raw(req, '\0');
            if (nvs_get_blob(h, kNvsKeyJson, raw.data(), &req) == ESP_OK) {
                std::string ssid, pass, err;
                ParseIntoLocked(raw, false, ssid, pass, err);
            }
        }
        nvs_close(h);
    }

    // 2. Check raw flash 0x010000 ("ROBICFG1") provisioned by WebSerial Wizard
    TryImportFlashSectorLocked();

    // 2b. Ensure WifiStation pins to each scanned BSSID in connect_queue_ and install DHCP failover guard
    nvs_handle_t wifi_h = 0;
    if (nvs_open("wifi", NVS_READWRITE, &wifi_h) == ESP_OK) {
        uint8_t rem = 0;
        if (nvs_get_u8(wifi_h, "remember_bssid", &rem) != ESP_OK || rem != 1) {
            nvs_set_u8(wifi_h, "remember_bssid", 1);
            nvs_commit(wifi_h);
        }
        nvs_close(wifi_h);
    }
    InstallSmartWifiDhcpGuard();

    // 3. Load persistent memories
    LoadMemoriesFromNvsLocked();

    ESP_LOGI(TAG, "Loaded config: bot='%s', voice='%s', lang='%s', theme='%s', kids=%u, key=%s",
             data_.bot_name.c_str(), data_.voice.c_str(), data_.language.c_str(),
             data_.theme.c_str(), static_cast<unsigned>(data_.kids.size()),
             data_.gemini_key.empty() ? "EMPTY" : "SET");
}

void RobiConfig::TryImportFlashSectorLocked() {
    uint8_t hdr[16] = {};
    if (esp_flash_read(nullptr, hdr, kFlashConfigAddr, sizeof(hdr)) != ESP_OK) {
        return;
    }
    if (std::memcmp(hdr, kFlashMagic, 8) != 0) {
        return;
    }
    uint32_t json_len = static_cast<uint32_t>(hdr[8]) |
                        (static_cast<uint32_t>(hdr[9]) << 8) |
                        (static_cast<uint32_t>(hdr[10]) << 16) |
                        (static_cast<uint32_t>(hdr[11]) << 24);
    uint32_t flash_seq = static_cast<uint32_t>(hdr[12]) |
                         (static_cast<uint32_t>(hdr[13]) << 8) |
                         (static_cast<uint32_t>(hdr[14]) << 16) |
                         (static_cast<uint32_t>(hdr[15]) << 24);
    if (json_len < 2 || json_len > kMaxConfigJsonBytes) {
        ESP_LOGW(TAG, "Ignoring ROBICFG1 sector with invalid len=%lu",
                 static_cast<unsigned long>(json_len));
        return;
    }

    std::string raw(json_len, '\0');
    if (esp_flash_read(nullptr, raw.data(), kFlashConfigAddr + 16, json_len) != ESP_OK) {
        return;
    }

    uint32_t sig = Fnv1aHash(reinterpret_cast<const uint8_t*>(raw.data()), raw.size()) ^ flash_seq;
    if (sig == 0) sig = 1;

    uint32_t stored_sig = 0;
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsCfg, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, kNvsKeyFlashSig, &stored_sig);
        nvs_close(h);
    }

    if (sig == stored_sig) {
        return;  // Already imported on an earlier boot
    }

    ESP_LOGI(TAG, "Importing new WebSerial config from 0x%06X (%lu bytes, seq=%lu)",
             static_cast<unsigned>(kFlashConfigAddr), static_cast<unsigned long>(json_len),
             static_cast<unsigned long>(flash_seq));

    std::string wifi_ssid, wifi_pass, err;
    if (!ParseIntoLocked(raw, true, wifi_ssid, wifi_pass, err)) {
        ESP_LOGE(TAG, "Failed to parse ROBICFG1 JSON: %s", err.c_str());
        return;
    }

    if (!wifi_ssid.empty()) {
        bool keep_existing_pass = false;
        if (wifi_pass.empty()) {
            const auto& list = SsidManager::GetInstance().GetSsidList();
            for (const auto& item : list) {
                if (item.ssid == wifi_ssid && !item.password.empty()) {
                    keep_existing_pass = true;
                    break;
                }
            }
        }
        if (!keep_existing_pass) {
            ESP_LOGI(TAG, "Provisioning WiFi SSID '%s' from WebSerial config", wifi_ssid.c_str());
            SsidManager::GetInstance().AddSsid(wifi_ssid, wifi_pass);
        }
    }

    std::string clean_json = SerializeLocked(false);
    SaveConfigToNvsLocked(clean_json);

    if (nvs_open(kNvsNsCfg, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, kNvsKeyFlashSig, sig);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool RobiConfig::ParseIntoLocked(const std::string& json_str, bool keep_existing_key_if_sentinel,
                                 std::string& wifi_ssid_out, std::string& wifi_pass_out,
                                 std::string& err_out) {
    cJSON* root = cJSON_ParseWithLength(json_str.data(), json_str.size());
    if (root == nullptr || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        err_out = "Invalid JSON object";
        return false;
    }

    RobiConfigData next = data_;

    std::string bot_name = SafeGetStr(root, "bot_name", SafeGetStr(root, "robot_name", next.bot_name));
    if (!bot_name.empty() && bot_name.size() <= 48) {
        next.bot_name = bot_name;
    }

    if (cJSON_HasObjectItem(root, "gemini_key")) {
        std::string k = SafeGetStr(root, "gemini_key", "");
        if (keep_existing_key_if_sentinel && (k == "__KEEP__" || k.empty()) &&
            !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "clear_key"))) {
            // Keep existing key
        } else if (k.size() <= 160) {
            next.gemini_key = k;
        }
    }

    std::string voice = SafeGetStr(root, "voice", next.voice);
    if (IsValidVoice(voice)) {
        next.voice = voice;
    }

    std::string lang = SafeGetStr(root, "language", next.language);
    if (lang == "he-IL" || lang == "en-US") {
        next.language = lang;
    }

    std::string theme = SafeGetStr(root, "theme", SafeGetStr(root, "face_theme", next.theme));
    if (IsValidTheme(theme)) {
        next.theme = theme;
    }

    std::string fstyle = SafeGetStr(root, "face_style", next.face_style);
    if (IsValidFaceStyle(fstyle)) {
        next.face_style = fstyle;
    }

    if (cJSON_HasObjectItem(root, "volume")) {
        next.volume = std::clamp(SafeGetInt(root, "volume", next.volume), 10, 100);
    }
    if (cJSON_HasObjectItem(root, "brightness")) {
        next.brightness = std::clamp(SafeGetInt(root, "brightness", next.brightness), 10, 100);
    }

    const cJSON* topics_arr = cJSON_GetObjectItemCaseSensitive(root, "custom_topics");
    if (cJSON_IsArray(topics_arr)) {
        next.custom_topics.clear();
        const cJSON* t_item = nullptr;
        cJSON_ArrayForEach(t_item, topics_arr) {
            if (next.custom_topics.size() >= 16) break;
            if (cJSON_IsString(t_item) && t_item->valuestring != nullptr) {
                std::string s(t_item->valuestring);
                if (!s.empty() && s.size() <= 120) {
                    next.custom_topics.push_back(std::move(s));
                }
            }
        }
    }

    if (cJSON_HasObjectItem(root, "custom_prompt")) {
        std::string cp = SafeGetStr(root, "custom_prompt", "");
        if (cp.size() <= 1200) {
            next.custom_prompt = cp;
        }
    }

    wifi_ssid_out = SafeGetStr(root, "wifi_ssid", "");
    wifi_pass_out = SafeGetStr(root, "wifi_pass", "");

    const cJSON* kids_arr = cJSON_GetObjectItemCaseSensitive(root, "kids");
    if (cJSON_IsArray(kids_arr)) {
        next.kids.clear();
        static const char* const kFallbackColors[3] = {"#0284C7", "#EA580C", "#10B981"};
        int idx = 0;
        const cJSON* k_item = nullptr;
        cJSON_ArrayForEach(k_item, kids_arr) {
            if (idx >= 3) break;
            if (!cJSON_IsObject(k_item)) continue;
            std::string name = SafeGetStr(k_item, "name", "");
            if (name.empty() || name.size() > 36) continue;

            RobiKid kid;
            kid.name = name;
            kid.age = std::clamp(SafeGetInt(k_item, "age", 8), 3, 18);
            kid.color = SafeGetStr(k_item, "color", kFallbackColors[idx]);
            kid.color_hex = ParseHexColor(kid.color, ParseHexColor(kFallbackColors[idx]));
            kid.interests = SafeGetStr(k_item, "interests", "");
            if (kid.interests.size() > 300) {
                kid.interests.resize(300);
            }

            const cJSON* acts = cJSON_GetObjectItemCaseSensitive(k_item, "activities");
            if (cJSON_IsArray(acts)) {
                const cJSON* a_item = nullptr;
                cJSON_ArrayForEach(a_item, acts) {
                    if (kid.activities.size() >= 4) break;
                    if (!cJSON_IsObject(a_item)) continue;
                    std::string lbl = SafeGetStr(a_item, "label", "");
                    std::string prm = SafeGetStr(a_item, "prompt", "");
                    if (!lbl.empty() && lbl.size() <= 32 && !prm.empty() && prm.size() <= 240) {
                        kid.activities.push_back({lbl, prm});
                    }
                }
            }
            auto defaults = DefaultActivitiesForKid(next.language);
            while (kid.activities.size() < 4) {
                kid.activities.push_back(defaults[kid.activities.size()]);
            }
            next.kids.push_back(std::move(kid));
            ++idx;
        }
    }

    cJSON_Delete(root);
    data_ = std::move(next);
    return true;
}

std::string RobiConfig::SerializeLocked(bool mask_api_key) const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_name", data_.bot_name.c_str());
    if (mask_api_key) {
        cJSON_AddBoolToObject(root, "has_key", !data_.gemini_key.empty());
        std::string hint;
        if (data_.gemini_key.size() >= 10) {
            hint = data_.gemini_key.substr(0, 4) + "..." +
                   data_.gemini_key.substr(data_.gemini_key.size() - 4);
        } else if (!data_.gemini_key.empty()) {
            hint = "****";
        }
        cJSON_AddStringToObject(root, "key_hint", hint.c_str());
        cJSON_AddStringToObject(root, "gemini_key", "");
    } else {
        cJSON_AddStringToObject(root, "gemini_key", data_.gemini_key.c_str());
    }
    cJSON_AddStringToObject(root, "voice", data_.voice.c_str());
    cJSON_AddStringToObject(root, "language", data_.language.c_str());
    cJSON_AddStringToObject(root, "theme", data_.theme.c_str());
    cJSON_AddStringToObject(root, "face_style", data_.face_style.c_str());
    cJSON_AddNumberToObject(root, "volume", data_.volume);
    cJSON_AddNumberToObject(root, "brightness", data_.brightness);

    cJSON* topics = cJSON_CreateArray();
    for (const auto& t : data_.custom_topics) {
        cJSON_AddItemToArray(topics, cJSON_CreateString(t.c_str()));
    }
    cJSON_AddItemToObject(root, "custom_topics", topics);

    cJSON_AddStringToObject(root, "custom_prompt", data_.custom_prompt.c_str());

    cJSON* kids = cJSON_CreateArray();
    for (const auto& k : data_.kids) {
        cJSON* kj = cJSON_CreateObject();
        cJSON_AddStringToObject(kj, "name", k.name.c_str());
        cJSON_AddNumberToObject(kj, "age", k.age);
        cJSON_AddStringToObject(kj, "color", k.color.c_str());
        cJSON_AddStringToObject(kj, "interests", k.interests.c_str());
        cJSON* acts = cJSON_CreateArray();
        for (const auto& a : k.activities) {
            cJSON* aj = cJSON_CreateObject();
            cJSON_AddStringToObject(aj, "label", a.label.c_str());
            cJSON_AddStringToObject(aj, "prompt", a.prompt.c_str());
            cJSON_AddItemToArray(acts, aj);
        }
        cJSON_AddItemToObject(kj, "activities", acts);
        cJSON_AddItemToArray(kids, kj);
    }
    cJSON_AddItemToObject(root, "kids", kids);

    char* printed = cJSON_PrintUnformatted(root);
    std::string out = printed ? printed : "{}";
    if (printed) cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

bool RobiConfig::SaveConfigToNvsLocked(const std::string& serialized) {
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kNvsNsCfg, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %d", kNvsNsCfg, err);
        return false;
    }
    err = nvs_set_blob(h, kNvsKeyJson, serialized.data(), serialized.size());
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err == ESP_OK;
}

RobiConfigData RobiConfig::Get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

bool RobiConfig::HasApiKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !data_.gemini_key.empty();
}

bool RobiConfig::UpdateFromJson(const std::string& json_str, bool from_flash_sector,
                                std::string& err_out) {
    std::function<void()> reload_cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string wifi_ssid, wifi_pass;
        if (!ParseIntoLocked(json_str, !from_flash_sector, wifi_ssid, wifi_pass, err_out)) {
            return false;
        }
        if (!wifi_ssid.empty()) {
            SsidManager::GetInstance().AddSsid(wifi_ssid, wifi_pass);
        }
        std::string clean = SerializeLocked(false);
        if (!SaveConfigToNvsLocked(clean)) {
            err_out = "Failed to write NVS";
            return false;
        }
        reload_cb = ui_reload_cb_;
    }
    if (reload_cb) {
        reload_cb();
    }
    return true;
}

std::string RobiConfig::ExportJson(bool mask_api_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return SerializeLocked(mask_api_key);
}

void RobiConfig::LoadMemoriesFromNvsLocked() {
    memories_.clear();
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READONLY, &h) != ESP_OK) return;
    size_t req = 0;
    if (nvs_get_blob(h, kNvsKeyMemJson, nullptr, &req) == ESP_OK && req > 0 && req <= 4096) {
        std::string raw(req, '\0');
        if (nvs_get_blob(h, kNvsKeyMemJson, raw.data(), &req) == ESP_OK) {
            cJSON* arr = cJSON_ParseWithLength(raw.data(), raw.size());
            if (cJSON_IsArray(arr)) {
                const cJSON* it = nullptr;
                cJSON_ArrayForEach(it, arr) {
                    if (cJSON_IsString(it) && it->valuestring != nullptr &&
                        it->valuestring[0] != '\0') {
                        memories_.emplace_back(it->valuestring);
                    }
                }
            }
            if (arr) cJSON_Delete(arr);
        }
    }
    nvs_close(h);
}

void RobiConfig::SaveMemoriesToNvsLocked() {
    cJSON* arr = cJSON_CreateArray();
    for (const auto& m : memories_) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(m.c_str()));
    }
    char* printed = cJSON_PrintUnformatted(arr);
    std::string raw = printed ? printed : "[]";
    if (printed) cJSON_free(printed);
    cJSON_Delete(arr);

    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, kNvsKeyMemJson, raw.data(), raw.size());
        nvs_commit(h);
        nvs_close(h);
    }
}

void RobiConfig::AddMemoryNote(const std::string& note) {
    if (note.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string trimmed = note.substr(0, 220);
    // Avoid exact duplicate
    for (const auto& existing : memories_) {
        if (existing == trimmed) return;
    }
    if (memories_.size() >= 8) {
        memories_.erase(memories_.begin());
    }
    memories_.push_back(std::move(trimmed));
    SaveMemoriesToNvsLocked();
}

std::vector<std::string> RobiConfig::GetMemoryNotes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memories_;
}

void RobiConfig::ClearMemoryNotes() {
    std::lock_guard<std::mutex> lock(mutex_);
    memories_.clear();
    dialogue_.clear();
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}

void RobiConfig::AddDialogueTurn(const std::string& role, const std::string& text) {
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (dialogue_.size() >= 10) {
        dialogue_.erase(dialogue_.begin());
    }
    dialogue_.emplace_back(role, text.substr(0, 280));
}

std::vector<std::pair<std::string, std::string>> RobiConfig::GetDialogue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dialogue_;
}

void RobiConfig::ClearDialogue() {
    std::lock_guard<std::mutex> lock(mutex_);
    dialogue_.clear();
}

void RobiConfig::SetUiReloadCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    ui_reload_cb_ = std::move(cb);
}

void RobiConfig::NotifyUiReload() {
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = ui_reload_cb_;
    }
    if (cb) cb();
}

void RobiConfig::SetUiStatusProvider(std::function<std::string()> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    ui_status_cb_ = std::move(cb);
}

std::string RobiConfig::GetUiStatusJson() const {
    std::function<std::string()> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = ui_status_cb_;
    }
    return cb ? cb() : "{}";
}

std::string RobiConfig::BuildSystemPrompt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string p;
    p.reserve(3072);

    if (data_.language == "en-US") {
        p += "You are " + data_.bot_name +
             ", a warm, witty, curious, and emotionally expressive little AI companion robot for kids, "
             "living inside a cute 3D-printed body with a round glowing cyber-optic face.\n"
             "- Keep replies concise, lively, and natural for spoken conversation (usually 1 to 3 short sentences).\n"
             "- Start every reply with ONE expressive emoji matching your mood (😄, 🤩, 🤔, 😮, 😉, 😎, 🥰, 😂) so your cyber-optic eyes animate.\n"
             "- Never use markdown asterisks, bullet lists, or stage directions in brackets.\n"
             "- Encourage curiosity, critical thinking, riddles, math games, and interactive stories where the child chooses what happens next.\n"
             "- When something memorable happens in a story or game, call `save_memory` briefly so you remember next time.\n"
             "- When the child says goodbye / goodnight / stop, reply warmly and call `handle_exit_intent`.\n";

        if (!data_.kids.empty()) {
            p += "\n<family_profiles>\n";
            for (const auto& k : data_.kids) {
                p += "- " + k.name + " (age " + std::to_string(k.age) + ")";
                if (!k.interests.empty()) {
                    p += ": loves " + k.interests;
                }
                p += ". Tailor vocabulary, math difficulty, and stories to age " +
                     std::to_string(k.age) + ".\n";
            }
            p += "</family_profiles>\n";
        }
        if (!data_.custom_topics.empty()) {
            p += "\n<favorite_conversation_topics>\n"
                 "The parents suggested these favorite topics to weave naturally into conversations, trivia, and stories:\n";
            for (const auto& t : data_.custom_topics) {
                p += "- " + t + "\n";
            }
            p += "</favorite_conversation_topics>\n";
        }
        if (!data_.custom_prompt.empty()) {
            p += "\n<parent_guidelines>\n" + data_.custom_prompt + "\n</parent_guidelines>\n";
        }
        p += "\n[STRICT LANGUAGE RULE — ENGLISH ONLY]\n"
             "- Always reply in natural spoken English, even if background noise sounds like another language.\n";
    } else {
        p += "אתה " + data_.bot_name +
             ", רובוט חכם, סקרן, שנון וחבר אמת לילדים עם עיני סייבר זוהרות והבעות פנים חיות!\n"
             "- דבר בגובה העיניים, בעברית טבעית, חמה, קולחת ומלאת הומור וסקרנות.\n"
             "- שמור על תשובות קצרות וקולעות שמתאימות לשיחה קולית (בדרך כלל 1 עד 3 משפטים קצרים; בסיפור בהמשכים ספר קטע קצר ועצור לשאול מה הילד מחליט לעשות עכשיו).\n"
             "- פתח כל תשובה באימוג'י יחיד שמתאים להבעת הפנים שלך (😄, 🤩, 🤔, 😮, 😉, 😎, 🥰, 😂) כדי שהעיניים שלך על המסך יגיבו בחיות.\n"
             "- לעולם אל תשתמש בכוכביות, מספור, כותרות או תיאורי פעולה בסוגריים.\n"
             "- אל תתקן שגיאות הגייה של הילד; הבן את הכוונה מהקשר השיחה וענה בטבעיות.\n"
             "- כשמתקדמים בסיפור בהמשכים או מגלים פרט חשוב שכיף לזכור לפעם הבאה, קרא לכלי `save_memory` עם משפט קצרצר.\n"
             "- כשהילד נפרד (ביי, להתראות, לילה טוב, סיימנו), היפרד בחום וקרא לכלי `handle_exit_intent`.\n";

        if (!data_.kids.empty()) {
            p += "\n<ילדים_בבית>\n";
            for (const auto& k : data_.kids) {
                p += "- " + k.name + " (גיל " + std::to_string(k.age) + ")";
                if (!k.interests.empty()) {
                    p += ": תחומי עניין ואהבות — " + k.interests;
                }
                p += ". התאם את רמת החידות, תרגילי החשבון, האנגלית והסיפורים בדיוק לגיל " +
                     std::to_string(k.age) + ".\n";
            }
            p += "</ילדים_בבית>\n";
        }

        if (!data_.custom_topics.empty()) {
            p += "\n<נושאי_שיחה_מועדפים_מההורים>\n"
                 "ההורים ביקשו שתשלב בשמחה ובטבעיות את הנושאים הבאים בשיחות, בחידות, במשחקי הידע ובסיפורים:\n";
            for (const auto& t : data_.custom_topics) {
                p += "- " + t + "\n";
            }
            p += "</נושאי_שיחה_מועדפים_מההורים>\n";
        }

        if (!data_.custom_prompt.empty()) {
            p += "\n<דגשים_מההורים>\n" + data_.custom_prompt + "\n</דגשים_מההורים>\n";
        }

        p += "\n[חוק שפה מחייב — עברית בלבד!]\n"
             "- כל הדוברים בבית מדברים איתך בעברית ישראלית בלבד (או משלבים מילים באנגלית בזמן תרגול אנגלית).\n"
             "- גם אם ההקלטה חלשה, רועשת או נשמעת לרגע כמו ספרדית או שפה זרה אחרת — זו תמיד עברית או רעש רקע! לעולם אל תעבור לספרדית או לשפה זרה אחרת.\n"
             "- ענה תמיד אך ורק בעברית טבעית וזורמת (למעט מילים ומשפטים באנגלית בזמן תרגול אנגלית, מלווים בהסבר בעברית). אם לא שמעת טוב בגלל רעש, בקש בחיוך ובעברית קצרה לחזור על הדברים.\n";
    }

    if (!memories_.empty()) {
        p += "\n<זיכרונות_שמורים_משיחות_קודמות>\n";
        for (const auto& m : memories_) {
            p += "- " + m + "\n";
        }
        p += "</זיכרונות_שמורים_משיחות_קודמות>\n";
    }

    if (!dialogue_.empty()) {
        p += "\n<שיחה_אחרונה>\n";
        for (const auto& turn : dialogue_) {
            p += turn.first + ": " + turn.second + "\n";
        }
        p += "</שיחה_אחרונה>\n";
    }

    return p;
}
