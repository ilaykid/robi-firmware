"""Patch a xiaozhi-esp32 checkout into Robi firmware.

usage: python patch_xiaozhi.py <xiaozhi_dir> <wake: heyrobby|hiesp>

- Hebrew UI (he-IL locale is chosen with build.py --language)
- LVGL right-to-left text (BIDI)
- Hebrew+Latin text font and Robi's faces in the assets partition
- dark theme by default (Robi's face is drawn for a black AMOLED)
- wake word: custom English "hey robby" (MultiNet) or preset "Hi ESP" (WakeNet)
- version ROBI_VERSION (env, default 2.5.4), higher than the device runs, for OTA
- no subtitles on the round screen (the face says it all); status line stays
- child profiles: two big buttons on the idle screen (Lavi / Ori); a tap starts a
  conversation that opens with "זה לביא" / "זה אורי" so Robi knows who is talking
- turn-based listening: device AEC off, so Robi listens, answers, then listens again
  (it never hears itself and the server's VAD decides when the child finished)
- after an OTA update, the device fetches Robi's assets (font, faces, wake word) once
  from the home server: <ota host>/xiaozhi/ota/download/robi-assets-<wake>.bin
"""
import json, os, re, sys
from pathlib import Path

fw = Path(sys.argv[1]).resolve()
wake = sys.argv[2]
board_cfg = fw / "main/boards/waveshare/esp32-s3-touch-amoled-1.75/config.json"


def must_replace(path: Path, old: str, new: str, count: int = 1):
    s = path.read_text(encoding="utf-8")
    if s.count(old) < 1:
        raise SystemExit(f"patch failed: '{old[:60]}' not found in {path}")
    path.write_text(s.replace(old, new, count), encoding="utf-8")
    print(f"patched {path.relative_to(fw)}")


# 1. sdkconfig additions for the standard 1.75 build (not the 1.75c one)
cfg = json.loads(board_cfg.read_text(encoding="utf-8"))
extra = ["CONFIG_LV_USE_BIDI=y", "CONFIG_LV_BIDI_DIR_AUTO=y"]
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

# 2. assets: our font + our faces (paths come from environment variables)
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

# 2b. round screen: subtitles inside the circle + child profile buttons
board_cc = fw / "main/boards/waveshare/esp32-s3-touch-amoled-1.75/esp32-s3-touch-amoled-1.75.cc"
must_replace(board_cc, '#include <lvgl.h>\n', '#include <lvgl.h>\n#include <cstring>\n#include "assets/lang_config.h"\n')
must_replace(
    board_cc,
    '        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n    }\n};\n',
    '        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n'
    '        RobiSetupUI();\n'
    '    }\n'
    '\n'
    '    // Robi: show the child profile buttons only while idle\n'
    '    virtual void SetStatus(const char* status) override {\n'
    '        SpiLcdDisplay::SetStatus(status);\n'
    '        DisplayLockGuard lock(this);\n'
    '        if (robi_profiles_ == nullptr) return;\n'
    '        if (status != nullptr && strcmp(status, Lang::Strings::STANDBY) == 0) {\n'
    '            lv_obj_remove_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);\n'
    '        } else {\n'
    '            lv_obj_add_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);\n'
    '        }\n'
    '    }\n'
    '\n'
    'private:\n'
    '    lv_obj_t* robi_profiles_ = nullptr;\n'
    '\n'
    '    static void OnProfile(lv_event_t* e) {\n'
    '        const char* who = static_cast<const char*>(lv_event_get_user_data(e));\n'
    '        lv_obj_t* bar = lv_obj_get_parent(static_cast<lv_obj_t*>(lv_event_get_current_target(e)));\n'
    '        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);\n'
    '        ESP_LOGI(TAG, "Robi profile: %s", who);\n'
    '        Application::GetInstance().WakeWordInvoke(who);\n'
    '    }\n'
    '\n'
    '    lv_obj_t* RobiButton(lv_obj_t* parent, const char* name, const char* says, uint32_t color) {\n'
    '        lv_obj_t* b = lv_button_create(parent);\n'
    '        lv_obj_set_size(b, LV_HOR_RES * 30 / 100, LV_VER_RES * 14 / 100);\n'
    '        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);\n'
    '        lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);\n'
    '        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);\n'
    '        lv_obj_set_style_shadow_width(b, 0, 0);\n'
    '        lv_obj_set_style_border_width(b, 0, 0);\n'
    '        lv_obj_t* l = lv_label_create(b);\n'
    '        lv_label_set_text(l, name);\n'
    '        lv_obj_set_style_text_color(l, lv_color_white(), 0);\n'
    '        lv_obj_center(l);\n'
    '        lv_obj_add_event_cb(b, OnProfile, LV_EVENT_CLICKED, (void*)says);\n'
    '        return b;\n'
    '    }\n'
    '\n'
    '    void RobiSetupUI() {\n'
    '        // subtitles: narrower than the screen and lifted a bit, so every line fits inside the circle\n'
    '        if (chat_message_label_ != nullptr && bottom_bar_ != nullptr) {\n'
    '            lv_obj_set_width(chat_message_label_, LV_HOR_RES * 70 / 100);\n'
    '            lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);\n'
    '            lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);\n'
    '            lv_obj_set_style_pad_bottom(bottom_bar_, LV_VER_RES * 9 / 100, 0);\n'
    '            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);\n'
    '        }\n'
    '        // two big buttons: who is talking\n'
    '        robi_profiles_ = lv_obj_create(lv_screen_active());\n'
    '        lv_obj_set_size(robi_profiles_, LV_HOR_RES * 80 / 100, LV_SIZE_CONTENT);\n'
    '        lv_obj_set_style_bg_opa(robi_profiles_, LV_OPA_TRANSP, 0);\n'
    '        lv_obj_set_style_border_width(robi_profiles_, 0, 0);\n'
    '        lv_obj_set_style_pad_all(robi_profiles_, 0, 0);\n'
    '        lv_obj_set_scrollbar_mode(robi_profiles_, LV_SCROLLBAR_MODE_OFF);\n'
    '        lv_obj_set_flex_flow(robi_profiles_, LV_FLEX_FLOW_ROW);\n'
    '        lv_obj_set_flex_align(robi_profiles_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);\n'
    '        lv_obj_align(robi_profiles_, LV_ALIGN_BOTTOM_MID, 0, -(LV_VER_RES * 10 / 100));\n'
    '        RobiButton(robi_profiles_, "\\u05dc\\u05d1\\u05d9\\u05d0", "\\u05d6\\u05d4 \\u05dc\\u05d1\\u05d9\\u05d0", 0x2E86DE);  // Lavi\n'
    '        RobiButton(robi_profiles_, "\\u05d0\\u05d5\\u05e8\\u05d9", "\\u05d6\\u05d4 \\u05d0\\u05d5\\u05e8\\u05d9", 0xF39C12);  // Ori\n'
    '        lv_obj_add_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);\n'
    '        // Robi: no subtitles, the face says it all (the text covered the face)\n'
    '        SetHideSubtitle(true);\n'
    '    }\n'
    '};\n',
)

# 3. dark theme by default
must_replace(fw / "main/display/lcd_display.cc",
             'settings.GetString("theme", "light")', 'settings.GetString("theme", "dark")')
# 4. version (must be higher than what the device runs, for OTA)
version = os.environ.get("ROBI_VERSION", "2.5.6")
must_replace(fw / "CMakeLists.txt", 'set(PROJECT_VER "2.5.2")', f'set(PROJECT_VER "{version}")')

# 5. optional: one-time download of Robi's assets from the home server after an app-only
#    OTA. Off by default - a stock device fell into an update/rollback loop with it.
#    Robi's assets go in once with the full USB image; later app OTAs keep them.
if os.environ.get("ROBI_AUTO_ASSETS") == "1":
    ROBI_ASSETS_VERSION = int(os.environ.get("ROBI_ASSETS_VERSION", "1"))
    app = fw / "main/application.cc"
    must_replace(
        app,
        '    std::string download_url = settings.GetString("download_url");\n',
        '    std::string download_url = settings.GetString("download_url");\n'
        '    // Robi: fetch Robi\'s assets (Hebrew font, faces, wake word) once from the home server\n'
        '    bool robi_assets_download = false;\n'
        f'    if (download_url.empty() && settings.GetInt("robi", 0) != {ROBI_ASSETS_VERSION}) {{\n'
        '        std::string ota_url = Settings("wifi", false).GetString("ota_url");\n'
        '        auto pos = ota_url.find("/xiaozhi/ota");\n'
        '        if (pos != std::string::npos) {\n'
        f'            download_url = ota_url.substr(0, pos) + "/xiaozhi/ota/download/robi-assets-{wake}.bin";\n'
        '            robi_assets_download = true;\n'
        '            ESP_LOGI(TAG, "Robi assets from %s", download_url.c_str());\n'
        '        }\n'
        '    }\n',
    )
    must_replace(
        app,
        '    // Apply assets\n    assets.Apply();\n',
        '    if (robi_assets_download) {\n'
        '        {\n'
        '            Settings robi("assets", true);\n'
        f'            robi.SetInt("robi", {ROBI_ASSETS_VERSION});\n'
        '        }\n'
        '        if (ota_) {\n'
        '            ota_->MarkCurrentVersionValid();  // keep the new app (rollback is enabled)\n'
        '        }\n'
        '        ESP_LOGI(TAG, "Robi assets installed, restarting");\n'
        '        vTaskDelay(pdMS_TO_TICKS(500));\n'
        '        esp_restart();\n'
        '    }\n'
        '    // Apply assets\n    assets.Apply();\n',
    )
print("done")
