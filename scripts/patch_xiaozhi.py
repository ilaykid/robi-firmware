"""Patch a xiaozhi-esp32 v2.5.2 checkout into standalone HeyRobi firmware.

Usage:
    python scripts/patch_xiaozhi.py <xiaozhi_dir> <wake: heyrobby|hiesp>

Key features:
- 100% Zero-Server standalone architecture: connects directly from ESP32-S3 over TLS WebSocket
  to Google Gemini Live API (BidiGenerateContent) with uncompressed 16kHz in / 24kHz out PCM
  and a 512 KB PSRAM jitter ring buffer.
- On-device configuration via NVS ("robi_cfg") + WebSerial flash sector 0x010000 ("ROBICFG1"),
  so a single pre-built firmware binary works for everyone without compiling C++.
- Built-in local Hebrew RTL Parent Management Portal (http://robi.local + LAN IP) with hot-reload
  for 5 Face Design Themes, Voices, Language lock, and 0..3 Child Profiles & Activity Menus.
- Rebrands WiFi setup hotspot from Xiaozhi-XXXX to Robi-XXXX and skips cloud activation.
- Hardware fixes for Waveshare ESP32-S3-Touch-AMOLED-1.75 (SKU 31261):
  - PowerSaveTimer disabled on USB power; battery dim at 180s / shutdown at 1800s.
  - CST9217 touch controller 100kHz I2C bus + RF TX burst transient error absorber.
"""
import json
import os
import shutil
import sys
from pathlib import Path

fw = Path(sys.argv[1]).resolve()
wake = sys.argv[2]
repo_root = Path(__file__).resolve().parent.parent
board_cfg = fw / "main/boards/waveshare/esp32-s3-touch-amoled-1.75/config.json"


def must_replace(path: Path, old: str, new: str, count: int = 1):
    s = path.read_text(encoding="utf-8")
    if s.count(old) < 1:
        raise SystemExit(f"patch failed: '{old[:70]}' not found in {path}")
    path.write_text(s.replace(old, new, count), encoding="utf-8")
    print(f"patched {path.relative_to(fw)}")


# 0. Copy standalone HeyRobi C++ modules into fw/main/robi/
robi_src_dir = repo_root / "firmware" / "robi"
robi_dst_dir = fw / "main" / "robi"
if robi_dst_dir.exists():
    shutil.rmtree(robi_dst_dir)
shutil.copytree(robi_src_dir, robi_dst_dir)
print(f"copied {robi_src_dir} -> {robi_dst_dir.relative_to(fw)}")

# Register HeyRobi sources and headers in main/CMakeLists.txt
main_cmake = fw / "main/CMakeLists.txt"
must_replace(
    main_cmake,
    '            "assets.cc"\n            "main.cc"\n            )',
    '            "assets.cc"\n            "main.cc"\n'
    '            "robi/robi_config.cc"\n'
    '            "robi/robi_gemini_live.cc"\n'
    '            "robi/robi_http_portal.cc"\n            )',
)
must_replace(
    main_cmake,
    'set(INCLUDE_DIRS "." "display"',
    'set(INCLUDE_DIRS "." "robi" "display"',
)
must_replace(
    main_cmake,
    "                        spi_flash\n                        mbedtls\n",
    "                        spi_flash\n                        nvs_flash\n"
    "                        esp_http_server\n                        mbedtls\n",
)
must_replace(
    main_cmake,
    "if(NOT CMAKE_BUILD_EARLY_EXPANSION)\n"
    "    target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++23>)\n"
    "endif()\n",
    "if(NOT CMAKE_BUILD_EARLY_EXPANSION)\n"
    "    target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++23>)\n"
    '    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_tls_conn_new_sync" "-Wl,--wrap=esp_tls_conn_read")\n'
    "endif()\n",
)

# 1. sdkconfig additions for standard Waveshare 1.75 build
cfg = json.loads(board_cfg.read_text(encoding="utf-8"))
extra = [
    "CONFIG_LV_USE_BIDI=y",
    "CONFIG_LV_BIDI_DIR_AUTO=y",
    "CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y",
    "CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=8",
    "CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32",
    "CONFIG_ESP_WIFI_RX_BA_WIN=16",
    "CONFIG_LWIP_TCP_WND_DEFAULT=32768",
    "CONFIG_LWIP_TCP_SND_BUF_DEFAULT=16384",
    "CONFIG_LWIP_TCP_RECVMBOX_SIZE=32",
    "CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=32",
]
if wake == "heyrobby":
    extra += [
        "CONFIG_USE_CUSTOM_WAKE_WORD=y",
        'CONFIG_CUSTOM_WAKE_WORD="hey robby"',
        'CONFIG_CUSTOM_WAKE_WORD_DISPLAY="hey robby"',
        "CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20",
        "CONFIG_SR_MN_CN_NONE=y",
        "CONFIG_SR_MN_EN_MULTINET7_QUANT=y",
    ]
if os.environ.get("ROBI_TURN_BASED", "1") == "1":
    extra += ["CONFIG_USE_DEVICE_AEC=n"]
extra += ["CONFIG_USE_MULTILINE_CHAT_MESSAGE=y"]

found = False
for b in cfg["builds"]:
    if b["name"] == "esp32-s3-touch-amoled-1.75":
        lines = b.get("sdkconfig_append", [])
        if "CONFIG_USE_DEVICE_AEC=n" in extra:
            lines = [l for l in lines if not l.startswith("CONFIG_USE_DEVICE_AEC=")]
        b["sdkconfig_append"] = lines + extra
        found = True
if not found:
    raise SystemExit("build esp32-s3-touch-amoled-1.75 not found in config.json")
board_cfg.write_text(json.dumps(cfg, indent=4, ensure_ascii=False), encoding="utf-8")
print("sdkconfig_append:", extra)

# 2. Custom Hebrew+Latin font and face assets
assets_py = fw / "scripts/build_default_assets.py"
must_replace(
    assets_py,
    "    if not builtin_text_font or 'basic' not in builtin_text_font:\n        return None\n",
    "    if os.environ.get('ROBI_TEXT_FONT'):\n        return os.environ['ROBI_TEXT_FONT']\n"
    "    if not builtin_text_font or 'basic' not in builtin_text_font:\n        return None\n",
)
must_replace(
    assets_py,
    "    if not default_emoji_collection:\n        return None\n",
    "    if os.environ.get('ROBI_EMOJI_DIR'):\n        return os.environ['ROBI_EMOJI_DIR']\n"
    "    if not default_emoji_collection:\n        return None\n",
)

# 3. Waveshare 1.75 board UI, PowerSaveTimer fix, and CST9217 100kHz I2C resilience
board_cc = fw / "main/boards/waveshare/esp32-s3-touch-amoled-1.75/esp32-s3-touch-amoled-1.75.cc"
must_replace(
    board_cc,
    "#include <lvgl.h>\n",
    "#include <lvgl.h>\n"
    "#include <algorithm>\n#include <cmath>\n#include <cstdint>\n#include <cstdlib>\n"
    "#include <cstring>\n#include <string>\n#include <vector>\n"
    "#include <esp_timer.h>\n#include <wifi_manager.h>\n"
    '#include "assets/lang_config.h"\n'
    '#include "robi/robi_config.h"\n',
)
robi_ui = (Path(__file__).resolve().parent / "robi_ui.inc").read_text(encoding="utf-8")
must_replace(
    board_cc,
    "        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n    }\n};\n",
    "        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n"
    "        RobiSetupUI();\n"
    "    }\n" + robi_ui + "};\n",
)

must_replace(
    board_cc,
    "    void InitializePowerSaveTimer() {\n"
    "        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);\n"
    "        power_save_timer_->OnEnterSleepMode([this]() {\n"
    "            GetDisplay()->SetPowerSaveMode(true);\n"
    "            GetBacklight()->SetBrightness(20); });\n"
    "        power_save_timer_->OnExitSleepMode([this]() {\n"
    "            GetDisplay()->SetPowerSaveMode(false);\n"
    "            GetBacklight()->RestoreBrightness(); });\n"
    "        power_save_timer_->OnShutdownRequest([this](){ \n"
    "            pmic_->PowerOff(); });\n"
    "        power_save_timer_->SetEnabled(true);\n"
    "    }\n",
    "    void InitializePowerSaveTimer() {\n"
    "        power_save_timer_ = new PowerSaveTimer(-1, 180, 1800);\n"
    "        power_save_timer_->OnEnterSleepMode([this]() {\n"
    "            GetDisplay()->SetPowerSaveMode(true);\n"
    "            GetBacklight()->SetBrightness(20); });\n"
    "        power_save_timer_->OnExitSleepMode([this]() {\n"
    "            GetDisplay()->SetPowerSaveMode(false);\n"
    "            GetBacklight()->RestoreBrightness(); });\n"
    "        power_save_timer_->OnShutdownRequest([this](){\n"
    "            pmic_->PowerOff(); });\n"
    "        power_save_timer_->SetEnabled(false);\n"
    "    }\n",
)

must_replace(
    board_cc,
    "        tp_io_config.scl_speed_hz = 400*  1000;\n"
    "        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));\n"
    '        ESP_LOGI(TAG, "Initialize touch controller");\n'
    "        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));\n",
    "        tp_io_config.scl_speed_hz = 100 * 1000;\n"
    "        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));\n"
    '        ESP_LOGI(TAG, "Initialize touch controller");\n'
    "        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));\n"
    "        static esp_err_t (*s_orig_tp_read_data)(esp_lcd_touch_handle_t) = nullptr;\n"
    "        s_orig_tp_read_data = tp->read_data;\n"
    "        tp->read_data = [](esp_lcd_touch_handle_t h) -> esp_err_t {\n"
    "            esp_err_t err = s_orig_tp_read_data ? s_orig_tp_read_data(h) : ESP_OK;\n"
    "            if (err != ESP_OK) {\n"
    "                portENTER_CRITICAL(&h->data.lock);\n"
    "                h->data.points = 0;\n"
    "                portEXIT_CRITICAL(&h->data.lock);\n"
    "                return ESP_OK;\n"
    "            }\n"
    "            return ESP_OK;\n"
    "        };\n",
)

# 4. WiFi provisioning import from 0x010000 + "Robi-XXXX" AP branding + clean port-80 handoff
wifi_cc = fw / "main/boards/common/wifi_board.cc"
must_replace(
    wifi_cc,
    '#include "wifi_board.h"\n',
    '#include "wifi_board.h"\n#include "robi/robi_config.h"\n#include "robi/robi_http_portal.h"\n',
)
must_replace(
    wifi_cc,
    "void WifiBoard::StartNetwork() {\n"
    "    auto& wifi_manager = WifiManager::GetInstance();\n\n"
    "    // Initialize WiFi manager\n"
    "    WifiManagerConfig config;\n"
    '    config.ssid_prefix = "Xiaozhi";\n'
    "    config.language = Lang::CODE;\n"
    "    config.show_ota_config = true;\n"
    "    config.show_sleep_config = true;\n\n"
    "    // Set a DHCP hostname so the router shows a friendly name instead of \"espressif\".\n"
    "    // Uses the same \"<prefix>-<last 2 MAC bytes>\" scheme as the config AP SSID.\n"
    "    uint8_t mac[6];\n"
    "    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {\n"
    "        char hostname[32];\n"
    '        snprintf(hostname, sizeof(hostname), "%s-%02X%02X", config.ssid_prefix.c_str(), mac[4], mac[5]);\n'
    "        config.station_hostname = hostname;\n"
    "    }\n",
    "void WifiBoard::StartNetwork() {\n"
    "    RobiConfig::GetInstance().EnsureLoaded();\n"
    "    auto& wifi_manager = WifiManager::GetInstance();\n\n"
    "    // Initialize WiFi manager\n"
    "    WifiManagerConfig config;\n"
    '    config.ssid_prefix = "Robi";\n'
    "    config.language = Lang::CODE;\n"
    "    config.show_ota_config = false;\n"
    "    config.show_sleep_config = true;\n"
    '    config.station_hostname = "robi";\n',
)
must_replace(
    wifi_cc,
    "            in_config_mode_ = false;\n"
    '            ESP_LOGI(TAG, "Connected to WiFi: %s", data.c_str());\n',
    "            in_config_mode_ = false;\n"
    '            ESP_LOGI(TAG, "Connected to WiFi: %s", data.c_str());\n'
    "            RobiHttpPortal::GetInstance().Start();\n",
)
must_replace(
    wifi_cc,
    "void WifiBoard::StartWifiConfigMode() {\n"
    "    in_config_mode_ = true;\n",
    "void WifiBoard::StartWifiConfigMode() {\n"
    "    RobiHttpPortal::GetInstance().Stop();\n"
    "    in_config_mode_ = true;\n",
)
must_replace(
    wifi_cc,
    "    WifiManager::GetInstance().SetPowerSaveLevel(wifi_level);\n",
    "    (void)wifi_level;\n"
    "    // Keep WiFi radio responsive in Standby so http://robi.local & LAN HTTP never drop packets\n"
    "    WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::PERFORMANCE);\n",
)

# 5. AudioService raw PCM fast-path (16kHz mic -> Gemini Live, 24kHz Gemini Live -> speaker)
audio_h = fw / "main/audio/audio_service.h"
must_replace(
    audio_h,
    "#define MAX_PLAYBACK_TASKS_IN_QUEUE 2\n",
    "#define MAX_PLAYBACK_TASKS_IN_QUEUE 8\n",
)
audio_cc = fw / "main/audio/audio_service.cc"
must_replace(
    audio_cc,
    '        "audio_output", 2048 * 2, this, 4, &audio_output_task_handle_);\n',
    '        "audio_output", 2048 * 2, this, 6, &audio_output_task_handle_);\n',
)
must_replace(
    audio_cc,
    '        "opus_codec", 2048 * 12, this, 2, &opus_codec_task_handle_);\n',
    '        "opus_codec", 2048 * 12, this, 5, &opus_codec_task_handle_);\n',
)
must_replace(
    audio_cc,
    "    audio_engine_->OnOutput([this](std::vector<int16_t>&& data) {\n"
    "        PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(data));\n"
    "    });\n",
    "    audio_engine_->OnOutput([this](std::vector<int16_t>&& data) {\n"
    "        auto packet = std::make_unique<AudioStreamPacket>();\n"
    "        packet->sample_rate = 16000;\n"
    "        packet->frame_duration = 0;\n"
    "        packet->payload.resize(data.size() * sizeof(int16_t));\n"
    "        std::memcpy(packet->payload.data(), data.data(), packet->payload.size());\n"
    "        {\n"
    "            std::lock_guard<std::mutex> lock2(audio_queue_mutex_);\n"
    "            if (audio_send_queue_.size() >= MAX_SEND_PACKETS_IN_QUEUE) {\n"
    "                audio_send_queue_.pop_front();\n"
    "            }\n"
    "            audio_send_queue_.push_back(std::move(packet));\n"
    "        }\n"
    "        if (callbacks_.on_send_queue_available) {\n"
    "            callbacks_.on_send_queue_available();\n"
    "        }\n"
    "    });\n",
)
must_replace(
    audio_cc,
    "            SetDecodeSampleRate(packet->sample_rate, packet->frame_duration);\n"
    "            bool decoded = false;\n"
    "            if (opus_decoder_ != nullptr) {\n",
    "            bool decoded = false;\n"
    "            if (packet->frame_duration == 0) {\n"
    "                task.pcm.resize(packet->payload.size() / sizeof(int16_t));\n"
    "                std::memcpy(task.pcm.data(), packet->payload.data(),\n"
    "                            task.pcm.size() * sizeof(int16_t));\n"
    "                decoded = !task.pcm.empty();\n"
    "            } else if ((SetDecodeSampleRate(packet->sample_rate, packet->frame_duration),\n"
    "                        opus_decoder_ != nullptr)) {\n",
)

# 6. Standalone Application boot (no cloud server activation, start local portal + GeminiLiveProtocol)
app_cc = fw / "main/application.cc"
must_replace(
    app_cc,
    "        case kDeviceStateSpeaking:\n"
    "            display->SetStatus(Lang::Strings::SPEAKING);\n\n"
    "            if (listening_mode_ != kListeningModeRealtime) {\n"
    "                audio_service_.EnableVoiceProcessing(false);\n"
    "                // Only AFE wake word can be detected in speaking mode\n"
    "                audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());\n"
    "            }\n",
    "        case kDeviceStateSpeaking:\n"
    "            display->SetStatus(Lang::Strings::SPEAKING);\n\n"
    "            if (listening_mode_ != kListeningModeRealtime) {\n"
    "                audio_service_.EnableVoiceProcessing(false);\n"
    "                // Disable MultiNet7 + FD-AEC while speaking so CPU & WiFi TLS receive are 100% unstarved\n"
    "                audio_service_.EnableWakeWordDetection(false);\n"
    "            }\n",
)
must_replace(
    app_cc,
    '#include "websocket_protocol.h"\n',
    '#include "websocket_protocol.h"\n'
    '#include "robi/robi_config.h"\n'
    '#include "robi/robi_gemini_live.h"\n'
    '#include "robi/robi_http_portal.h"\n',
)
must_replace(
    app_cc,
    "void Application::ActivationTask() {\n"
    "    // Create OTA object for activation process\n"
    "    ota_ = std::make_unique<Ota>();\n\n"
    "    // Check for new assets version\n"
    "    CheckAssetsVersion();\n\n"
    "    // Check for new firmware version\n"
    "    CheckNewVersion();\n\n"
    "    // Initialize the protocol\n"
    "    InitializeProtocol();\n\n"
    "    // Signal completion to main loop\n"
    "    xEventGroupSetBits(event_group_, MAIN_EVENT_ACTIVATION_DONE);\n"
    "}\n",
    "void Application::ActivationTask() {\n"
    "    ota_ = std::make_unique<Ota>();\n"
    "    CheckAssetsVersion();\n"
    "    ota_->MarkCurrentVersionValid();\n"
    "    RobiHttpPortal::GetInstance().Start();\n"
    "    RobiConfig::GetInstance().NotifyUiReload();\n"
    "    InitializeProtocol();\n"
    "    xEventGroupSetBits(event_group_, MAIN_EVENT_ACTIVATION_DONE);\n"
    "}\n",
)
must_replace(
    app_cc,
    "    if (ota_->HasMqttConfig()) {\n"
    "        protocol_ = std::make_unique<MqttProtocol>();\n"
    "    } else if (ota_->HasWebsocketConfig()) {\n"
    "        protocol_ = std::make_unique<WebsocketProtocol>();\n"
    "    } else {\n"
    '        ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");\n'
    "        protocol_ = std::make_unique<MqttProtocol>();\n"
    "    }\n",
    "    protocol_ = std::make_unique<GeminiLiveProtocol>();\n",
)
must_replace(
    app_cc,
    "#if CONFIG_SEND_WAKE_WORD_DATA\n"
    "    // Encode and send the wake word data to the server\n"
    "    while (auto packet = audio_service_.PopWakeWordPacket()) {\n"
    "        protocol_->SendAudio(std::move(packet));\n"
    "    }\n"
    "    // Set the chat state to wake word detected\n"
    "    protocol_->SendWakeWordDetected(wake_word);\n"
    "    SetListeningMode(GetDefaultListeningMode());\n"
    "#else\n"
    "    // Set flag to play popup sound after state changes to listening\n"
    "    // (PlaySound here would be cleared by ResetDecoder in EnableVoiceProcessing)\n"
    "    play_popup_on_listening_ = true;\n"
    "    SetListeningMode(GetDefaultListeningMode());\n"
    "#endif\n",
    "    protocol_->SendWakeWordDetected(wake_word);\n"
    '    bool is_spoken_wake = (wake_word == "hey robby" || wake_word == "hey robi" ||\n'
    '                           wake_word == "Hi,ESP" || wake_word == "hi esp" || wake_word == "hiesp");\n'
    "    play_popup_on_listening_ = is_spoken_wake;\n"
    "    SetListeningMode(GetDefaultListeningMode());\n",
)

# 7. Dark AMOLED theme + black boot fill + 24KB LVGL task stack
lcd_cc = fw / "main/display/lcd_display.cc"
must_replace(lcd_cc, 'settings.GetString("theme", "light")', 'settings.GetString("theme", "dark")')
must_replace(
    lcd_cc,
    "std::vector<uint16_t> buffer(width_, 0xFFFF);",
    "std::vector<uint16_t> buffer(width_, 0x0000);",
    count=3,
)
must_replace(
    lcd_cc,
    "    port_cfg.task_priority = 1;\n",
    "    port_cfg.task_priority = 1;\n    port_cfg.task_stack = 24576;\n",
    count=3,
)

# 8. Firmware version
version = os.environ.get("ROBI_VERSION", "3.0.0")
must_replace(fw / "CMakeLists.txt", 'set(PROJECT_VER "2.5.2")', f'set(PROJECT_VER "{version}")')

print("HeyRobi standalone patch completed successfully!")
