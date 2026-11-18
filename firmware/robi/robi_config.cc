#include "robi_config.h"
#include "robi_utf8.h"

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

extern "C" {

esp_err_t __real_esp_wifi_scan_start(const wifi_scan_config_t* config, bool block);

esp_err_t __wrap_esp_wifi_scan_start(const wifi_scan_config_t* config, bool block) {
    if (config != nullptr && SsidManager::GetInstance().GetSsidList().size() > 1) {
        // When multiple WiFi networks are saved (e.g. Home WiFi + Phone Hotspot whose channel
        // changes dynamically across 1/6/11), always scan all channels so nearby Hotspots are found!
        return __real_esp_wifi_scan_start(nullptr, block);
    }
    return __real_esp_wifi_scan_start(config, block);
}

}  // extern "C"

namespace {

constexpr const char* kNvsNsCfg = "robi_cfg";
constexpr const char* kNvsKeyJson = "json";
constexpr const char* kNvsKeyFlashSig = "flash_sig";
constexpr const char* kNvsKeyGoodBssid = "good_bssid";
constexpr const char* kNvsKeyGoodSsid = "good_ssid";
constexpr const char* kNvsNsMem = "robi_mem";
constexpr const char* kNvsKeyMemJson = "notes";
constexpr const char* kNvsKeyScopedMemJson = "scoped_v2";
constexpr const char* kNvsKeyDlgJson = "dlg";
constexpr const char* kNvsKeyScopedDlgJson = "dlg_v2";
constexpr const char* kNvsKeyUsageJson = "usage";
constexpr const char* kFlashMagic = "ROBICFG1";

// Smart Multi-AP DHCP Failover Guard:
// If a mesh extender/repeater associates at 802.11 L2 (WIFI_EVENT_STA_CONNECTED) but fails to
// hand out a DHCP IP within 4.5s, blacklist that BSSID and immediately advance WifiStation's
// connect_queue_ to the working router BSSID, and remember the working BSSID in NVS.
bool s_wifi_guard_installed = false;
esp_timer_handle_t s_dhcp_watchdog_timer = nullptr;
esp_timer_handle_t s_nvs_flush_timer = nullptr;
bool s_mem_dirty = false;
bool s_dlg_dirty = false;
bool s_usage_dirty = false;
std::string s_cur_ssid;
std::array<uint8_t, 6> s_cur_bssid = {};
std::string s_good_ssid;
std::array<uint8_t, 6> s_good_bssid = {};
bool s_good_bssid_valid = false;
bool s_steering_to_good = false;
bool s_deliberate_skip_disconnect = false;
int s_direct_pin_fails = 0;
int s_fast_skip_count = 0;
std::vector<std::array<uint8_t, 6>> s_dead_dhcp_bssids;

bool IsDeadDhcpBssid(const std::array<uint8_t, 6>& b) {
    for (const auto& d : s_dead_dhcp_bssids) {
        if (d == b) return true;
    }
    return false;
}

bool HasVerifiedBssidForCurrentSsid() {
    return s_good_bssid_valid && !s_good_ssid.empty() && s_cur_ssid == s_good_ssid;
}

void SteerWifiConfigToGoodBssid() {
    if (!HasVerifiedBssidForCurrentSsid() || s_direct_pin_fails >= 2) return;
    wifi_config_t wcfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &wcfg) == ESP_OK) {
        std::string cfg_ssid(reinterpret_cast<const char*>(wcfg.sta.ssid),
                             strnlen(reinterpret_cast<const char*>(wcfg.sta.ssid), sizeof(wcfg.sta.ssid)));
        if (cfg_ssid != s_good_ssid) {
            return;
        }
        std::memcpy(wcfg.sta.bssid, s_good_bssid.data(), 6);
        wcfg.sta.bssid_set = 1;
        if (esp_wifi_set_config(WIFI_IF_STA, &wcfg) == ESP_OK) {
            s_steering_to_good = true;
        }
    }
}

void InstallSmartWifiDhcpGuard() {
    if (s_wifi_guard_installed) return;
    s_wifi_guard_installed = true;

    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsCfg, NVS_READONLY, &h) == ESP_OK) {
        size_t len = 6;
        char ssid_buf[36] = {};
        size_t ssid_len = sizeof(ssid_buf);
        if (nvs_get_blob(h, kNvsKeyGoodBssid, s_good_bssid.data(), &len) == ESP_OK && len == 6 &&
            nvs_get_str(h, kNvsKeyGoodSsid, ssid_buf, &ssid_len) == ESP_OK && ssid_buf[0] != '\0') {
            s_good_ssid = ssid_buf;
            s_good_bssid_valid = true;
            ESP_LOGI(TAG, "Loaded last good DHCP BSSID for '%s': %02x:%02x:%02x:%02x:%02x:%02x",
                     s_good_ssid.c_str(),
                     s_good_bssid[0], s_good_bssid[1], s_good_bssid[2],
                     s_good_bssid[3], s_good_bssid[4], s_good_bssid[5]);
        }
        nvs_close(h);
    }

    esp_event_loop_create_default();

    esp_timer_create_args_t targs = {};
    targs.callback = [](void*) {
        // Never blacklist or forcibly disconnect our verified working router BSSID when RSSI is weak (-83 dBm)
        if (HasVerifiedBssidForCurrentSsid() && s_cur_bssid == s_good_bssid) {
            ESP_LOGW(TAG, "DHCP still pending after 4.5s on verified router %02x:%02x:%02x:%02x:%02x:%02x — waiting naturally",
                     s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                     s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5]);
            return;
        }
        if (!IsDeadDhcpBssid(s_cur_bssid)) {
            ESP_LOGW(TAG, "DHCP timeout on '%s' BSSID %02x:%02x:%02x:%02x:%02x:%02x — blacklisting extender!",
                     s_cur_ssid.c_str(),
                     s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                     s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5]);
            if (s_dead_dhcp_bssids.size() < 8) {
                s_dead_dhcp_bssids.push_back(s_cur_bssid);
            }
        }
        SteerWifiConfigToGoodBssid();
        s_deliberate_skip_disconnect = true;
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
                size_t slen = ev->ssid_len > 0
                                  ? std::min<size_t>(ev->ssid_len, sizeof(ev->ssid))
                                  : strnlen(reinterpret_cast<const char*>(ev->ssid), sizeof(ev->ssid));
                s_cur_ssid.assign(reinterpret_cast<const char*>(ev->ssid), slen);
                std::memcpy(s_cur_bssid.data(), ev->bssid, 6);
                s_deliberate_skip_disconnect = false;
                if (s_dhcp_watchdog_timer == nullptr) return;
                esp_timer_stop(s_dhcp_watchdog_timer);

                if (HasVerifiedBssidForCurrentSsid() && s_cur_bssid == s_good_bssid) {
                    s_steering_to_good = false;
                    s_direct_pin_fails = 0;
                    return;
                }

                if (IsDeadDhcpBssid(s_cur_bssid)) {
                    ESP_LOGW(TAG, "Fast-skipping dead DHCP BSSID %02x:%02x:%02x:%02x:%02x:%02x on '%s'",
                             s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                             s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5], s_cur_ssid.c_str());
                    SteerWifiConfigToGoodBssid();
                    esp_timer_start_once(s_dhcp_watchdog_timer, 25000ULL);
                    return;
                }
                // ONLY fast-skip non-preferred BSSIDs within the SAME SSID (e.g. home router vs dead extender),
                // NEVER kick Robi off a different saved network such as a Phone Hotspot!
                if (HasVerifiedBssidForCurrentSsid() && s_cur_bssid != s_good_bssid && s_fast_skip_count < 12) {
                    ++s_fast_skip_count;
                    ESP_LOGI(TAG, "Fast-skipping repeater BSSID %02x:%02x:%02x:%02x:%02x:%02x on '%s' (%d/12) -> steering to router",
                             s_cur_bssid[0], s_cur_bssid[1], s_cur_bssid[2],
                             s_cur_bssid[3], s_cur_bssid[4], s_cur_bssid[5],
                             s_cur_ssid.c_str(), s_fast_skip_count);
                    SteerWifiConfigToGoodBssid();
                    esp_timer_start_once(s_dhcp_watchdog_timer, 25000ULL);
                    return;
                }
                esp_timer_start_once(s_dhcp_watchdog_timer, 4500000ULL);
            } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                if (s_dhcp_watchdog_timer != nullptr) {
                    esp_timer_stop(s_dhcp_watchdog_timer);
                }
                if (s_steering_to_good && !s_deliberate_skip_disconnect) {
                    ++s_direct_pin_fails;
                    if (s_direct_pin_fails >= 2) {
                        s_steering_to_good = false;
                        wifi_config_t wcfg = {};
                        if (esp_wifi_get_config(WIFI_IF_STA, &wcfg) == ESP_OK && wcfg.sta.bssid_set) {
                            wcfg.sta.bssid_set = 0;
                            std::memset(wcfg.sta.bssid, 0, 6);
                            esp_wifi_set_config(WIFI_IF_STA, &wcfg);
                        }
                        ESP_LOGW(TAG, "Preferred router BSSID unreachable; restored open BSSID roaming");
                    }
                }
                s_deliberate_skip_disconnect = false;
            }
        },
        nullptr, nullptr);

    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void*, esp_event_base_t, int32_t, void*) {
            if (s_dhcp_watchdog_timer != nullptr) {
                esp_timer_stop(s_dhcp_watchdog_timer);
            }
            s_fast_skip_count = 0;
            s_direct_pin_fails = 0;
            s_steering_to_good = false;
            s_deliberate_skip_disconnect = false;
            s_dead_dhcp_bssids.erase(
                std::remove(s_dead_dhcp_bssids.begin(), s_dead_dhcp_bssids.end(), s_cur_bssid),
                s_dead_dhcp_bssids.end());
            // Save verified router BSSID ONLY when it already had a dead-repeater skip or when
            // no verified BSSID exists yet (avoids overwriting home router BSSID when visiting a Hotspot,
            // unless that SSID itself needed repeater disambiguation!)
            if (!s_cur_ssid.empty() &&
                (!s_good_bssid_valid || s_good_ssid == s_cur_ssid || !s_dead_dhcp_bssids.empty())) {
                if (!s_good_bssid_valid || s_good_ssid != s_cur_ssid || s_good_bssid != s_cur_bssid) {
                    s_good_ssid = s_cur_ssid;
                    s_good_bssid = s_cur_bssid;
                    s_good_bssid_valid = true;
                    nvs_handle_t nh = 0;
                    if (nvs_open(kNvsNsCfg, NVS_READWRITE, &nh) == ESP_OK) {
                        nvs_set_str(nh, kNvsKeyGoodSsid, s_good_ssid.c_str());
                        nvs_set_blob(nh, kNvsKeyGoodBssid, s_good_bssid.data(), 6);
                        nvs_commit(nh);
                        nvs_close(nh);
                    }
                    ESP_LOGI(TAG, "Saved verified DHCP BSSID for '%s': %02x:%02x:%02x:%02x:%02x:%02x",
                             s_good_ssid.c_str(),
                             s_good_bssid[0], s_good_bssid[1], s_good_bssid[2],
                             s_good_bssid[3], s_good_bssid[4], s_good_bssid[5]);
                }
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

bool IsValidPreferredModel(const std::string& m) {
    return m == "auto" || m == "gemini-3.8-live" ||
           m == "gemini-2.5-flash-native-audio-preview-12-2025" ||
           m == "gemini-2.0-flash-live-001";
}

bool IsValidPersonaMode(const std::string& pm) {
    return pm == "kids" || pm == "assistant" || pm == "hybrid";
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

    // 2a. Auto-heal inaudible ES8311 speaker volume (<= 25% = -64..-77 dB on ES8311 DAC, caused by legacy RTL range slider)
    if (data_.volume <= 25) {
        ESP_LOGW(TAG, "Healing inaudible ES8311 speaker volume (%d%% -> 80%%)", data_.volume);
        data_.volume = 80;
        SaveConfigToNvsLocked(SerializeLocked(false));
    }

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

    // 3. Load persistent memories and daily model usage (with UTF-8 self-healing & legacy migration)
    LoadMemoriesFromNvsLocked();
    LoadUsageFromNvsLocked();
    if (active_scope_.empty()) {
        active_scope_ = EffectiveScopeLocked();
    }

    ESP_LOGI(TAG, "Loaded config: bot='%s', mode='%s', model='%s', voice='%s', lang='%s', theme='%s', kids=%u, key=%s",
             data_.bot_name.c_str(), data_.persona_mode.c_str(), data_.preferred_model.c_str(), data_.voice.c_str(),
             data_.language.c_str(), data_.theme.c_str(), static_cast<unsigned>(data_.kids.size()),
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

    uint32_t erase_len = ((16u + json_len + 4095u) / 4096u) * 4096u;
    if (sig == stored_sig) {
        esp_flash_erase_region(nullptr, kFlashConfigAddr, erase_len);
        ESP_LOGI(TAG, "Wiped legacy ROBICFG1 staging sector at 0x%06X (%lu bytes)",
                 static_cast<unsigned>(kFlashConfigAddr), static_cast<unsigned long>(erase_len));
        return;
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
    if (SaveConfigToNvsLocked(clean_json)) {
        if (nvs_open(kNvsNsCfg, NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_u32(h, kNvsKeyFlashSig, sig);
            nvs_commit(h);
            nvs_close(h);
        }
        esp_flash_erase_region(nullptr, kFlashConfigAddr, erase_len);
        ESP_LOGI(TAG, "Erased ROBICFG1 staging sector at 0x%06X (%lu bytes) after NVS import",
                 static_cast<unsigned>(kFlashConfigAddr), static_cast<unsigned long>(erase_len));
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

    std::string bot_name = robi::Utf8SafeTruncate(
        SafeGetStr(root, "bot_name", SafeGetStr(root, "robot_name", next.bot_name)), 48);
    if (!bot_name.empty()) {
        next.bot_name = bot_name;
    }

    if (cJSON_HasObjectItem(root, "gemini_key")) {
        std::string k = SafeGetStr(root, "gemini_key", "");
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "clear_key"))) {
            next.gemini_key.clear();
        } else if (keep_existing_key_if_sentinel && (k == "__KEEP__" || k.size() < 20)) {
            // Keep existing key (ignores blank fields AND short browser password-autofill strings!)
        } else if (k.size() >= 20 && k.size() <= 160) {
            next.gemini_key = robi::Utf8Sanitize(k);
        }
    }

    std::string pref_model = SafeGetStr(root, "preferred_model", next.preferred_model);
    if (IsValidPreferredModel(pref_model)) {
        next.preferred_model = pref_model;
    }

    bool has_explicit_pm = cJSON_HasObjectItem(root, "persona_mode");
    std::string pmode = SafeGetStr(root, "persona_mode", next.persona_mode);
    if (IsValidPersonaMode(pmode)) {
        next.persona_mode = pmode;
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
        int min_vol = keep_existing_key_if_sentinel ? 35 : 0;
        next.volume = std::clamp(SafeGetInt(root, "volume", next.volume), min_vol, 100);
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
                std::string s = robi::Utf8SafeTruncate(t_item->valuestring, 120);
                if (!s.empty()) {
                    next.custom_topics.push_back(std::move(s));
                }
            }
        }
    }

    if (cJSON_HasObjectItem(root, "custom_prompt")) {
        next.custom_prompt = robi::Utf8SafeTruncate(SafeGetStr(root, "custom_prompt", ""), 1200);
    }

    wifi_ssid_out = robi::Utf8SafeTruncate(SafeGetStr(root, "wifi_ssid", ""), 64);
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
            std::string name = robi::Utf8SafeTruncate(SafeGetStr(k_item, "name", ""), 36);
            if (name.empty()) continue;

            RobiKid kid;
            kid.name = std::move(name);
            kid.age = std::clamp(SafeGetInt(k_item, "age", 8), 3, 18);
            kid.color = SafeGetStr(k_item, "color", kFallbackColors[idx]);
            kid.color_hex = ParseHexColor(kid.color, ParseHexColor(kFallbackColors[idx]));
            kid.interests = robi::Utf8SafeTruncate(SafeGetStr(k_item, "interests", ""), 300);

            const cJSON* acts = cJSON_GetObjectItemCaseSensitive(k_item, "activities");
            if (cJSON_IsArray(acts)) {
                const cJSON* a_item = nullptr;
                cJSON_ArrayForEach(a_item, acts) {
                    if (kid.activities.size() >= 4) break;
                    if (!cJSON_IsObject(a_item)) continue;
                    std::string lbl = robi::Utf8SafeTruncate(SafeGetStr(a_item, "label", ""), 32);
                    std::string prm = robi::Utf8SafeTruncate(SafeGetStr(a_item, "prompt", ""), 240);
                    if (!lbl.empty() && !prm.empty()) {
                        kid.activities.push_back({std::move(lbl), std::move(prm)});
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
        if (!has_explicit_pm) {
            next.persona_mode = next.kids.empty() ? "assistant" : "hybrid";
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
    cJSON_AddStringToObject(root, "preferred_model", data_.preferred_model.c_str());
    cJSON_AddStringToObject(root, "persona_mode", data_.persona_mode.c_str());
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

bool RobiConfig::HasKids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !data_.kids.empty();
}

bool RobiConfig::UpdateFromJson(const std::string& json_str, bool from_flash_sector,
                                std::string& err_out, bool* session_params_changed_out) {
    std::function<void()> reload_cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        RobiConfigData before = data_;
        std::string wifi_ssid, wifi_pass;
        if (!ParseIntoLocked(json_str, !from_flash_sector, wifi_ssid, wifi_pass, err_out)) {
            return false;
        }
        if (session_params_changed_out != nullptr) {
            *session_params_changed_out = !before.HasSameSessionParams(data_);
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

void RobiConfig::ScheduleNvsFlushLocked() {
    if (s_nvs_flush_timer == nullptr) {
        esp_timer_create_args_t targs = {};
        targs.callback = [](void*) { RobiConfig::GetInstance().FlushNvsFromInternalStack(); };
        targs.dispatch_method = ESP_TIMER_TASK;
        targs.name = "robi_nvs_fl";
        esp_timer_create(&targs, &s_nvs_flush_timer);
    }
    if (s_nvs_flush_timer != nullptr) {
        esp_timer_stop(s_nvs_flush_timer);
        esp_timer_start_once(s_nvs_flush_timer, 80000ULL);  // 80ms debounce on Internal SRAM timer task
    }
}

std::string RobiConfig::EffectiveScopeLocked() const {
    if (data_.persona_mode == "assistant") {
        return "assistant";
    }
    if (data_.persona_mode == "kids") {
        if (!active_scope_.empty() && active_scope_.rfind("kid:", 0) == 0) {
            return active_scope_;
        }
        if (data_.kids.size() == 1 && !data_.kids[0].name.empty()) {
            return "kid:" + data_.kids[0].name;
        }
        return "kids";
    }
    // "hybrid": default wake / face tap = "assistant", kid button = "kid:<Name>"
    if (!active_scope_.empty() &&
        (active_scope_ == "assistant" || active_scope_.rfind("kid:", 0) == 0 || active_scope_ == "kids")) {
        return active_scope_;
    }
    return "assistant";
}

void RobiConfig::SetActiveScope(const std::string& scope) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_scope_ = robi::Utf8SafeTruncate(scope, 48);
}

std::string RobiConfig::GetActiveScope() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return EffectiveScopeLocked();
}

void RobiConfig::ActivateDefaultWakeScope() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (data_.persona_mode == "kids") {
        if (data_.kids.size() == 1 && !data_.kids[0].name.empty()) {
            active_scope_ = "kid:" + data_.kids[0].name;
        } else {
            active_scope_ = "kids";
        }
    } else {
        active_scope_ = "assistant";
    }
}

void RobiConfig::ActivateKidScope(size_t kid_idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (kid_idx < data_.kids.size() && !data_.kids[kid_idx].name.empty()) {
        active_scope_ = "kid:" + data_.kids[kid_idx].name;
    } else {
        active_scope_ = "kids";
    }
}

std::vector<std::string>& RobiConfig::FindOrCreateScopeMemoriesLocked(const std::string& scope) {
    for (auto& kv : scoped_memories_) {
        if (kv.first == scope) return kv.second;
    }
    scoped_memories_.push_back({scope, {}});
    return scoped_memories_.back().second;
}

std::vector<RobiConfig::TurnPair>& RobiConfig::FindOrCreateScopeDialogueLocked(const std::string& scope) {
    for (auto& kv : scoped_dialogue_) {
        if (kv.first == scope) return kv.second;
    }
    scoped_dialogue_.push_back({scope, {}});
    return scoped_dialogue_.back().second;
}

void RobiConfig::LoadMemoriesFromNvsLocked() {
    scoped_memories_.clear();
    scoped_dialogue_.clear();
    bool healed_utf8 = false;
    bool migrated_legacy = false;

    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READONLY, &h) != ESP_OK) return;

    size_t sreq = 0;
    if (nvs_get_blob(h, kNvsKeyScopedMemJson, nullptr, &sreq) == ESP_OK && sreq > 0 && sreq <= 6144) {
        std::string sraw(sreq, '\0');
        if (nvs_get_blob(h, kNvsKeyScopedMemJson, sraw.data(), &sreq) == ESP_OK) {
            cJSON* obj = cJSON_ParseWithLength(sraw.data(), sraw.size());
            if (cJSON_IsObject(obj)) {
                const cJSON* scope_arr = nullptr;
                cJSON_ArrayForEach(scope_arr, obj) {
                    if (!scope_arr->string || !cJSON_IsArray(scope_arr)) continue;
                    std::string sc = robi::Utf8SafeTruncate(scope_arr->string, 48);
                    if (sc.empty()) continue;
                    auto& vec = FindOrCreateScopeMemoriesLocked(sc);
                    const cJSON* it = nullptr;
                    cJSON_ArrayForEach(it, scope_arr) {
                        if (cJSON_IsString(it) && it->valuestring != nullptr && it->valuestring[0] != '\0') {
                            std::string orig(it->valuestring);
                            std::string clean = robi::Utf8SafeTruncate(orig, 220);
                            if (clean != orig) healed_utf8 = true;
                            if (!clean.empty() && vec.size() < 20) {
                                vec.push_back(std::move(clean));
                            }
                        }
                    }
                }
            }
            if (obj) cJSON_Delete(obj);
        }
    } else {
        // Migrate v3.1.x flat "notes" array into scoped per-kid / assistant vaults seamlessly!
        size_t req = 0;
        if (nvs_get_blob(h, kNvsKeyMemJson, nullptr, &req) == ESP_OK && req > 0 && req <= 4096) {
            std::string raw(req, '\0');
            if (nvs_get_blob(h, kNvsKeyMemJson, raw.data(), &req) == ESP_OK) {
                cJSON* arr = cJSON_ParseWithLength(raw.data(), raw.size());
                if (cJSON_IsArray(arr)) {
                    const cJSON* it = nullptr;
                    cJSON_ArrayForEach(it, arr) {
                        if (cJSON_IsString(it) && it->valuestring != nullptr && it->valuestring[0] != '\0') {
                            std::string clean = robi::Utf8SafeTruncate(it->valuestring, 220);
                            if (clean.empty()) continue;
                            std::string target_scope;
                            for (const auto& k : data_.kids) {
                                if (!k.name.empty() && clean.find(k.name) != std::string::npos) {
                                    target_scope = "kid:" + k.name;
                                    break;
                                }
                            }
                            if (target_scope.empty()) {
                                target_scope = data_.kids.empty()
                                                   ? "assistant"
                                                   : (data_.kids.size() == 1 ? ("kid:" + data_.kids[0].name) : "kids");
                            }
                            auto& vec = FindOrCreateScopeMemoriesLocked(target_scope);
                            if (vec.size() < 20) {
                                vec.push_back(std::move(clean));
                                migrated_legacy = true;
                            }
                        }
                    }
                }
                if (arr) cJSON_Delete(arr);
            }
        }
    }

    size_t dreq = 0;
    if (nvs_get_blob(h, kNvsKeyScopedDlgJson, nullptr, &dreq) == ESP_OK && dreq > 0 && dreq <= 6144) {
        std::string draw(dreq, '\0');
        if (nvs_get_blob(h, kNvsKeyScopedDlgJson, draw.data(), &dreq) == ESP_OK) {
            cJSON* dobj = cJSON_ParseWithLength(draw.data(), draw.size());
            if (cJSON_IsObject(dobj)) {
                const cJSON* scope_arr = nullptr;
                cJSON_ArrayForEach(scope_arr, dobj) {
                    if (!scope_arr->string || !cJSON_IsArray(scope_arr)) continue;
                    std::string sc = robi::Utf8SafeTruncate(scope_arr->string, 48);
                    if (sc.empty()) continue;
                    auto& dvec = FindOrCreateScopeDialogueLocked(sc);
                    const cJSON* it = nullptr;
                    cJSON_ArrayForEach(it, scope_arr) {
                        if (cJSON_IsObject(it)) {
                            std::string r = robi::Utf8SafeTruncate(SafeGetStr(it, "r", ""), 16);
                            std::string orig_t = SafeGetStr(it, "t", "");
                            std::string t = robi::Utf8SafeTruncate(orig_t, 240);
                            if (t != orig_t) healed_utf8 = true;
                            if (!r.empty() && !t.empty() && dvec.size() < 12) {
                                dvec.emplace_back(std::move(r), std::move(t));
                            }
                        }
                    }
                }
            }
            if (dobj) cJSON_Delete(dobj);
        }
    }
    nvs_close(h);

    if (healed_utf8 || migrated_legacy) {
        ESP_LOGI(TAG, "Persisting scoped v3.2 memory vaults (migrated=%d, healed=%d)",
                 migrated_legacy ? 1 : 0, healed_utf8 ? 1 : 0);
        s_mem_dirty = true;
        s_dlg_dirty = true;
        ScheduleNvsFlushLocked();
    }
}

void RobiConfig::FlushNvsFromInternalStack() {
    std::string mem_raw;
    std::string dlg_raw;
    std::string usage_raw;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (s_mem_dirty) {
            s_mem_dirty = false;
            cJSON* obj = cJSON_CreateObject();
            for (const auto& kv : scoped_memories_) {
                if (kv.second.empty()) continue;
                cJSON* arr = cJSON_CreateArray();
                for (const auto& m : kv.second) {
                    cJSON_AddItemToArray(arr, cJSON_CreateString(m.c_str()));
                }
                cJSON_AddItemToObject(obj, kv.first.c_str(), arr);
            }
            char* printed = cJSON_PrintUnformatted(obj);
            mem_raw = printed ? printed : "{}";
            if (printed) cJSON_free(printed);
            cJSON_Delete(obj);
        }
        if (s_dlg_dirty) {
            s_dlg_dirty = false;
            cJSON* dobj = cJSON_CreateObject();
            for (const auto& kv : scoped_dialogue_) {
                if (kv.second.empty()) continue;
                cJSON* darr = cJSON_CreateArray();
                for (const auto& turn : kv.second) {
                    cJSON* item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "r", turn.first.c_str());
                    cJSON_AddStringToObject(item, "t", turn.second.c_str());
                    cJSON_AddItemToArray(darr, item);
                }
                cJSON_AddItemToObject(dobj, kv.first.c_str(), darr);
            }
            char* printed = cJSON_PrintUnformatted(dobj);
            dlg_raw = printed ? printed : "{}";
            if (printed) cJSON_free(printed);
            cJSON_Delete(dobj);
        }
        if (s_usage_dirty) {
            s_usage_dirty = false;
            cJSON* obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "day", usage_day_.c_str());
            cJSON* arr = cJSON_CreateArray();
            for (const auto& mu : model_usage_) {
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", mu.id.c_str());
                cJSON_AddNumberToObject(item, "s", mu.sessions);
                cJSON_AddNumberToObject(item, "t", mu.turns);
                cJSON_AddNumberToObject(item, "a", mu.audio_sec);
                cJSON_AddNumberToObject(item, "tok", mu.tokens);
                cJSON_AddItemToArray(arr, item);
            }
            cJSON_AddItemToObject(obj, "models", arr);
            char* printed = cJSON_PrintUnformatted(obj);
            usage_raw = printed ? printed : "{}";
            if (printed) cJSON_free(printed);
            cJSON_Delete(obj);
        }
    }

    if (mem_raw.empty() && dlg_raw.empty() && usage_raw.empty()) return;

    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READWRITE, &h) == ESP_OK) {
        if (!mem_raw.empty()) {
            nvs_set_blob(h, kNvsKeyScopedMemJson, mem_raw.data(), mem_raw.size());
        }
        if (!dlg_raw.empty()) {
            nvs_set_blob(h, kNvsKeyScopedDlgJson, dlg_raw.data(), dlg_raw.size());
        }
        if (!usage_raw.empty()) {
            nvs_set_blob(h, kNvsKeyUsageJson, usage_raw.data(), usage_raw.size());
        }
        nvs_commit(h);
        nvs_close(h);
    }
}

void RobiConfig::SaveMemoriesToNvsLocked() {
    s_mem_dirty = true;
    ScheduleNvsFlushLocked();
}

void RobiConfig::AddMemoryNote(const std::string& note) {
    std::string trimmed = robi::Utf8SafeTruncate(note, 220);
    if (trimmed.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sc = EffectiveScopeLocked();
    auto& vec = FindOrCreateScopeMemoriesLocked(sc);
    for (const auto& existing : vec) {
        if (existing == trimmed) return;
    }
    if (vec.size() >= 18) {
        vec.erase(vec.begin());
    }
    vec.push_back(std::move(trimmed));
    SaveMemoriesToNvsLocked();
}

std::vector<std::string> RobiConfig::GetMemoryNotes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> all;
    const bool en = (data_.language == "en-US");
    for (const auto& kv : scoped_memories_) {
        std::string badge;
        if (kv.first == "assistant") {
            badge = en ? "[Assistant] " : "[עוזר אישי] ";
        } else if (kv.first.rfind("kid:", 0) == 0) {
            badge = "[" + kv.first.substr(4) + "] ";
        } else {
            badge = en ? "[Kids] " : "[ילדים] ";
        }
        for (const auto& m : kv.second) {
            all.push_back(badge + m);
        }
    }
    return all;
}

void RobiConfig::ClearMemoryNotes() {
    std::lock_guard<std::mutex> lock(mutex_);
    scoped_memories_.clear();
    scoped_dialogue_.clear();
    s_mem_dirty = false;
    s_dlg_dirty = false;
    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, kNvsKeyMemJson);
        nvs_erase_key(h, kNvsKeyScopedMemJson);
        nvs_erase_key(h, kNvsKeyDlgJson);
        nvs_erase_key(h, kNvsKeyScopedDlgJson);
        nvs_commit(h);
        nvs_close(h);
    }
}

void RobiConfig::LoadUsageFromNvsLocked() {
    static const char* const kKnownModels[3] = {
        "gemini-3.8-live",
        "gemini-2.5-flash-native-audio-preview-12-2025",
        "gemini-2.0-flash-live-001",
    };
    model_usage_.clear();
    for (const char* m : kKnownModels) {
        model_usage_.push_back({m, 0, 0, 0, 0});
    }

    nvs_handle_t h = 0;
    if (nvs_open(kNvsNsMem, NVS_READONLY, &h) != ESP_OK) return;
    size_t req = 0;
    if (nvs_get_blob(h, kNvsKeyUsageJson, nullptr, &req) == ESP_OK && req > 0 && req <= 4096) {
        std::string raw(req, '\0');
        if (nvs_get_blob(h, kNvsKeyUsageJson, raw.data(), &req) == ESP_OK) {
            cJSON* obj = cJSON_ParseWithLength(raw.data(), raw.size());
            if (cJSON_IsObject(obj)) {
                std::string day = robi::Utf8SafeTruncate(SafeGetStr(obj, "day", "today"), 16);
                if (!day.empty()) {
                    usage_day_ = day;
                }
                const cJSON* models_arr = cJSON_GetObjectItemCaseSensitive(obj, "models");
                if (cJSON_IsArray(models_arr)) {
                    const cJSON* it = nullptr;
                    cJSON_ArrayForEach(it, models_arr) {
                        if (!cJSON_IsObject(it)) continue;
                        std::string id = SafeGetStr(it, "id", "");
                        if (id.empty()) continue;
                        auto& mu = FindOrCreateModelUsageLocked(id);
                        mu.sessions = static_cast<uint32_t>(std::max(0, SafeGetInt(it, "s", 0)));
                        mu.turns = static_cast<uint32_t>(std::max(0, SafeGetInt(it, "t", 0)));
                        mu.audio_sec = static_cast<uint32_t>(std::max(0, SafeGetInt(it, "a", 0)));
                        mu.tokens = static_cast<uint32_t>(std::max(0, SafeGetInt(it, "tok", 0)));
                    }
                }
            }
            if (obj) cJSON_Delete(obj);
        }
    }
    nvs_close(h);
}

void RobiConfig::SaveUsageToNvsLocked() {
    s_usage_dirty = true;
    ScheduleNvsFlushLocked();
}

RobiModelUsage& RobiConfig::FindOrCreateModelUsageLocked(const std::string& model_id) {
    for (auto& mu : model_usage_) {
        if (mu.id == model_id) return mu;
    }
    model_usage_.push_back({model_id, 0, 0, 0, 0});
    return model_usage_.back();
}

void RobiConfig::SyncUsageDay(const std::string& client_day_iso) {
    if (client_day_iso.size() != 10 || client_day_iso[4] != '-' || client_day_iso[7] != '-') {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (usage_day_ == client_day_iso) {
        return;
    }
    if (usage_day_ == "today" || usage_day_.empty()) {
        usage_day_ = client_day_iso;
        SaveUsageToNvsLocked();
        return;
    }
    ESP_LOGI(TAG, "New calendar day detected (%s -> %s): resetting daily model quota counters",
             usage_day_.c_str(), client_day_iso.c_str());
    usage_day_ = client_day_iso;
    for (auto& mu : model_usage_) {
        mu.sessions = 0;
        mu.turns = 0;
        mu.audio_sec = 0;
        mu.tokens = 0;
    }
    SaveUsageToNvsLocked();
}

void RobiConfig::RecordModelSession(const std::string& model_id) {
    if (model_id.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& mu = FindOrCreateModelUsageLocked(model_id);
    ++mu.sessions;
    SaveUsageToNvsLocked();
}

void RobiConfig::RecordModelTurn(const std::string& model_id, uint32_t audio_sec_delta,
                                 uint32_t tokens_delta) {
    if (model_id.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& mu = FindOrCreateModelUsageLocked(model_id);
    ++mu.turns;
    mu.audio_sec += audio_sec_delta;
    mu.tokens += tokens_delta;
    SaveUsageToNvsLocked();
}

void RobiConfig::ResetDailyUsage() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& mu : model_usage_) {
        mu.sessions = 0;
        mu.turns = 0;
        mu.audio_sec = 0;
        mu.tokens = 0;
    }
    SaveUsageToNvsLocked();
}

std::string RobiConfig::GetUsageDay() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return usage_day_;
}

std::vector<RobiModelUsage> RobiConfig::GetModelUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_usage_;
}

void RobiConfig::AddDialogueTurn(const std::string& role, const std::string& text) {
    std::string safe_role = robi::Utf8SafeTruncate(role, 16);
    std::string safe_text = robi::Utf8SafeTruncate(text, 240);
    if (safe_role.empty() || safe_text.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string sc = EffectiveScopeLocked();
    auto& dvec = FindOrCreateScopeDialogueLocked(sc);
    if (dvec.size() >= 12) {
        dvec.erase(dvec.begin());
    }
    dvec.emplace_back(std::move(safe_role), std::move(safe_text));
    s_dlg_dirty = true;
    ScheduleNvsFlushLocked();
}

std::vector<std::pair<std::string, std::string>> RobiConfig::GetDialogue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sc = EffectiveScopeLocked();
    for (const auto& kv : scoped_dialogue_) {
        if (kv.first == sc) return kv.second;
    }
    return {};
}

void RobiConfig::ClearDialogue() {
    std::lock_guard<std::mutex> lock(mutex_);
    scoped_dialogue_.clear();
    s_dlg_dirty = true;
    ScheduleNvsFlushLocked();
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
    p.reserve(4096);

    const std::string scope = EffectiveScopeLocked();
    const bool is_assistant = (scope == "assistant");
    std::string active_kid_name;
    if (scope.rfind("kid:", 0) == 0) {
        active_kid_name = scope.substr(4);
    }

    if (data_.language == "en-US") {
        if (is_assistant) {
            p += "You are " + data_.bot_name +
                 ", a sharp, fast, witty, and genuinely helpful desktop AI personal assistant "
                 "with a round glowing cyber-optic AMOLED face.\n"
                 "- Speak concisely, directly, and naturally (1 to 3 crisp spoken sentences unless asked for detail). Avoid childish tone.\n"
                 "- Start replies with ONE expressive emoji (😄, 🤔, 😮, 😉, 😎, 🤩) or call `set_emotion` when shifting mood.\n"
                 "- Never use markdown asterisks, bullet lists, or stage directions in brackets.\n"
                 "- LIVE WEB SEARCH (`googleSearch`): Automatically use Google Search for weather, live sports scores, news, dates, prices, or current facts, and state the answer directly.\n"
                 "- INTERACTIVE SCREEN GAMES: If asked for trivia on screen, call `show_trivia_card` with 2-4 touch options as you read the question. If asked to play 'Heads Up' / forehead charades on any topic, call `start_heads_up_round` with 15 great words and give a 1-sentence kickoff.\n"
                 "- MANDATORY VOICE REPLY RULE: Every audio turn is addressed directly to you — never stay silent! If audio is muffled, briefly ask to repeat.\n"
                 "- Use `save_memory` ONLY for meaningful new personal preferences, ongoing projects, or important facts (after speaking). Call `handle_exit_intent` after saying goodbye.\n";
            if (data_.persona_mode == "hybrid" && !data_.kids.empty()) {
                p += "\n<household_kids_context>\n"
                     "By default in this session you are speaking to the adult/parent. However, if one of the household kids explicitly introduces themselves by name (";
                for (size_t i = 0; i < data_.kids.size(); ++i) {
                    if (i > 0) p += ", ";
                    p += data_.kids[i].name + " age " + std::to_string(data_.kids[i].age);
                }
                p += "), warmly adapt your tone and difficulty to their age!\n</household_kids_context>\n";
            }
        } else {
            p += "You are " + data_.bot_name +
                 ", a warm, witty, curious, and emotionally expressive little AI companion robot for kids, "
                 "living inside a cute 3D-printed body with a round glowing cyber-optic face.\n"
                 "- Keep replies concise, lively, and natural for spoken conversation (usually 1 to 3 short sentences).\n"
                 "- Start every reply with ONE expressive emoji matching your mood (😄, 🤩, 🤔, 😮, 😉, 😎, 🥰, 😂) and feel free to call `set_emotion` during stories or jokes.\n"
                 "- Never use markdown asterisks, bullet lists, or stage directions in brackets.\n"
                 "- LIVE WEB SEARCH (`googleSearch`): Use Google Search for real-time weather, sports, animals, space, or facts, always phrased safely and engagingly for kids.\n"
                 "- SCREEN TRIVIA & FOREHEAD GAMES:\n"
                 "  * When playing screen trivia (solo or head-to-head), ALWAYS call `show_trivia_card` with the short question, 2-4 short button options, `correct_index` (0-based), `player_turn`, and `score_summary` right as you ask the question out loud!\n"
                 "  * When the child asks to play 'Heads Up' / forehead game on a topic, generate 15 fun age-appropriate words and call `start_heads_up_round`, then say a 1-sentence cheer to hold Robi on their forehead!\n"
                 "- MANDATORY VOICE REPLY RULE — NEVER STAY SILENT: Every audio turn is 100% addressed to you — NEVER stay silent even on 1-word answers ('yes', 'seven'). If noise makes speech unintelligible, cheerfully ask them to repeat in one short sentence.\n"
                 "- LEARN & REMEMBER THE CHILD: Weave saved memories naturally into conversation. Call `save_memory` ONLY for meaningful NEW personal facts or story milestones, AFTER speaking.\n"
                 "- When the child says goodbye / goodnight / stop, reply warmly and call `handle_exit_intent`.\n";

            if (!active_kid_name.empty()) {
                p += "\n<active_child_in_this_session>Currently speaking with: " + active_kid_name + "</active_child_in_this_session>\n";
            }
            if (!data_.kids.empty()) {
                p += "\n<family_profiles>\n";
                for (const auto& k : data_.kids) {
                    p += "- " + k.name + " (age " + std::to_string(k.age) + ")";
                    if (!k.interests.empty()) p += ": loves " + k.interests;
                    p += ". Tailor vocabulary and difficulty to age " + std::to_string(k.age) + ".\n";
                }
                p += "</family_profiles>\n";
            }
        }
        if (!data_.custom_topics.empty()) {
            p += "\n<favorite_topics>\n";
            for (const auto& t : data_.custom_topics) p += "- " + t + "\n";
            p += "</favorite_topics>\n";
        }
        if (!data_.custom_prompt.empty()) {
            p += "\n<custom_guidelines>\n" + data_.custom_prompt + "\n</custom_guidelines>\n";
        }
        p += "\n[STRICT LANGUAGE RULE — ENGLISH ONLY]\n"
             "- Always reply in natural spoken English.\n";
    } else {
        if (is_assistant) {
            p += "אתה " + data_.bot_name +
                 ", עוזר אישי שולחני חכם, חד, שנון ותכליתי לדוברי עברית, החי בתוך רובוט שולחני קומפקטי עם מסך עגול ועיני סייבר זוהרות.\n"
                 "- דבר בעברית טבעית, בגובה העיניים של מבוגר, בצורה עניינית, חכמה וקולחת (ללא טון ילדותי).\n"
                 "- שמור על תשובות ממוקדות ותמציתיות שמתאימות לשיחה קולית (בדרך כלל 1 עד 3 משפטים חדים וברורים, אלא אם התבקשת להרחיב).\n"
                 "- פתח תשובות באימוג'י יחיד מתאים (😄, 🤔, 😮, 😉, 😎, 🤩) או קרא לכלי `set_emotion` כשמתאימה הבעת פנים חיה.\n"
                 "- לעולם אל תשתמש בכוכביות, מספור, כותרות או תיאורי פעולה בסוגריים.\n"
                 "- חיפוש חי בזמן אמת (`googleSearch`): היעזר אוטומטית בחיפוש גוגל בכל שאלה על מזג אוויר, חדשות, תוצאות ספורט, מועדים, מחירים או עובדות עדכניות, ותן תשובה מדויקת ועדכנית.\n"
                 "- משחקי מסך וחיישן תנועה כשמבקשים:\n"
                 "  * אם מבקשים לשחק טריוויה על המסך (יחיד או ראש בראש), קרא בכל שאלה לכלי `show_trivia_card` עם השאלה, 2–4 אפשרויות קצרות לכפתורים, `correct_index` וסיכום הניקוד תוך כדי הקראת השאלה!\n"
                 "  * אם מבקשים לשחק 'נחש מה על המצח' בנושא כלשהו, צור מיד 15 מילים מעולות בעברית באותו נושא וקרא לכלי `start_heads_up_round` בליווי משפט פתיחה קצר להניח אותך על המצח!\n"
                 "- חוק מענה קולי מחייב — לעולם אל תשתוק!: כל קטע קול מופנה אליך ישירות ומצפה למענה קולי. אם בגלל רעש לא ניתן להבין מה נאמר, בקש בקצרה לחזור על הדברים.\n"
                 "- זיכרון אישי: שלב בטבעיות פרטים מתוך `<זיכרונות_שמורים>` וקרא ל-`save_memory` רק כשעולה העדפה, פרויקט או פרט חשוב חדש לשמירה לטווח ארוך (לאחר סיום התשובה הקולית). בסיום שיחה קרא ל-`handle_exit_intent`.\n";

            if (data_.persona_mode == "hybrid" && !data_.kids.empty()) {
                p += "\n<הקשר_משפחתי_במצב_משולב>\n"
                     "ברירת המחדל בסשן זה היא שיחה עם מבוגר כעוזר אישי ענייני. עם זאת, אם אחד הילדים בבית מציג את עצמו בשמו בקול (";
                for (size_t i = 0; i < data_.kids.size(); ++i) {
                    if (i > 0) p += ", ";
                    p += data_.kids[i].name + " גיל " + std::to_string(data_.kids[i].age);
                }
                p += "), עבור מיד לטון חם, שמח ומותאם לגילו!\n</הקשר_משפחתי_במצב_משולב>\n";
            }
        } else {
            p += "אתה " + data_.bot_name +
                 ", רובוט חכם, סקרן, שנון וחבר אמת לילדים עם עיני סייבר זוהרות והבעות פנים חיות!\n"
                 "- דבר בגובה העיניים, בעברית טבעית, חמה, קולחת ומלאת הומור וסקרנות.\n"
                 "- שמור על תשובות קצרות וקולעות שמתאימות לשיחה קולית (בדרך כלל 1 עד 3 משפטים קצרים; בסיפור בהמשכים ספר קטע קצר ועצור לשאול מה הילד מחליט לעשות עכשיו).\n"
                 "- פתח כל תשובה באימוג'י יחיד שמתאים להבעת הפנים שלך (😄, 🤩, 🤔, 😮, 😉, 😎, 🥰, 😂), והחלף הבעות פנים בחופשיות עם `set_emotion` ברגעי שיא, צחוק, קריצה או הפתעה!\n"
                 "- לעולם אל תשתמש בכוכביות, מספור, כותרות או תיאורי פעולה בסוגריים.\n"
                 "- אל תתקן שגיאות הגייה של הילד; הבן את הכוונה מהקשר השיחה וענה בטבעיות.\n"
                 "- חיפוש חי בגוגל (`googleSearch`): השתמש בחיפוש גוגל לשאלות על מזג אוויר, תוצאות משחקים, חלל, חיות ועובדות חדשות, והצג את התשובה בצורה בטוחה, מרתקת ומותאמת לגיל הילד.\n"
                 "- הנחיית משחקי מסך וחיישן תנועה:\n"
                 "  * כשמשחקים טריוויה על המסך (יחיד או ראש בראש בין אחים), קרא בכל שאלה לכלי `show_trivia_card` עם נוסח שאלה קצר, 2–4 אפשרויות קצרות לכפתורי המגע, אינדקס התשובה הנכונה `correct_index` (`0` עד `3`), תור השחקן וסיכום הניקוד — ממש בזמן שאתה שואל בקול! כשמתקבלת הודעה על הלחיצה של השחקן, הגב בהתלהבות במשפט אחד והצג מיד את השאלה הבאה עם `show_trivia_card`!\n"
                 "  * כשמבקשים לשחק 'נחש מה על המצח' בנושא כלשהו, שאל איזה נושא אם לא צוין, וברגע שיש נושא צור 15 מילים כיפיות ומוכרות בעברית באותו נושא וקרא לכלי `start_heads_up_round` יחד עם משפט עידוד קצר אחד להחזיק אותך על המצח!\n"
                 "- חוק מענה קולי מחייב — לעולם אל תשתוק!: כל קטע קול שמגיע אליך מופנה אליך ב-100% ומצפה לתשובה קולית חיה ממך, גם אם הילד ענה במילה קצרה אחת ('כן', 'לא', 'שבע'). אם בגלל רעש ממש לא ניתן להבין מה נאמר, שאל בחיוך במשפט קצר אחד כמו 'אופס, לא שמעתי טוב, מה אמרת?'.\n"
                 "- זיכרון מתפתח והיכרות אישית עם הילד: שלב באופן טבעי פרטים מתוך `<זיכרונות_שמורים>` ומתוך `<שיחה_אחרונה>`. קרא לכלי `save_memory` אך ורק כשמתגלה פרט אישי חדש ומשמעותי על הילד או בסיום פרק בסיפור בהמשכים, ותמיד רק בסוף התשובה הקולית שלך.\n"
                 "- כשהילד נפרד (ביי, להתראות, לילה טוב, סיימנו), היפרד בחום וקרא לכלי `handle_exit_intent`.\n";

            if (!active_kid_name.empty()) {
                p += "\n<הילד_הפעיל_בשיחה_זו>כרגע מדבר איתך: " + active_kid_name + " — פנה אליו בשמו והתאם הכל לגילו ולתחומי העניין שלו!</הילד_הפעיל_בשיחה_זו>\n";
            }

            if (!data_.kids.empty()) {
                p += "\n<ילדים_בבית>\n";
                for (const auto& k : data_.kids) {
                    p += "- " + k.name + " (גיל " + std::to_string(k.age) + ")";
                    if (!k.interests.empty()) {
                        p += ": תחומי עניין ואהבות — " + k.interests;
                    }
                    p += ". התאם את רמת החידות, הטריוויה, החשבון והסיפורים בדיוק לגיל " +
                         std::to_string(k.age) + ".\n";
                }
                p += "</ילדים_בבית>\n";
            }
        }

        if (!data_.custom_topics.empty()) {
            p += "\n<נושאי_שיחה_מועדפים>\n";
            for (const auto& t : data_.custom_topics) {
                p += "- " + t + "\n";
            }
            p += "</נושאי_שיחה_מועדפים>\n";
        }

        if (!data_.custom_prompt.empty()) {
            p += "\n<דגשים_אישיים>\n" + data_.custom_prompt + "\n</דגשים_אישיים>\n";
        }

        p += "\n[חוק שפה מחייב — עברית בלבד!]\n"
             "- כל הדוברים בבית מדברים איתך בעברית ישראלית בלבד (או משלבים מילים באנגלית כשמבקשים לתרגל אנגלית).\n"
             "- גם אם ההקלטה חלשה או רועשת ונשמעת לרגע כמו שפה זרה — זו תמיד עברית או רעש רקע! לעולם אל תעבור לספרדית או לשפה זרה אחרת.\n";
    }

    // Inject ONLY memories belonging to the active persona scope (plus shared "kids" notes for child sessions)
    std::vector<std::string> active_mems;
    for (const auto& kv : scoped_memories_) {
        if (kv.first == scope || (!is_assistant && scope != "kids" && kv.first == "kids")) {
            for (const auto& m : kv.second) {
                active_mems.push_back(m);
            }
        }
    }
    if (!active_mems.empty()) {
        p += "\n<זיכרונות_שמורים>\n";
        for (const auto& m : active_mems) {
            p += "- " + m + "\n";
        }
        p += "</זיכרונות_שמורים>\n";
    }

    for (const auto& kv : scoped_dialogue_) {
        if (kv.first == scope && !kv.second.empty()) {
            p += "\n<שיחה_אחרונה>\n";
            for (const auto& turn : kv.second) {
                p += turn.first + ": " + turn.second + "\n";
            }
            p += "</שיחה_אחרונה>\n";
            break;
        }
    }

    return robi::Utf8Sanitize(p);
}
