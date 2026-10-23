"""Patch a xiaozhi-esp32 checkout into Robi firmware.

usage: python patch_xiaozhi.py <xiaozhi_dir> <wake: heyrobby|hiesp>

- Hebrew UI (he-IL locale is chosen with build.py --language)
- LVGL right-to-left text (BIDI)
- Hebrew+Latin text font and Robi's faces in the assets partition
- dark theme by default (Robi's face is drawn for a black AMOLED)
- wake word: custom English "hey robby" (MultiNet) or preset "Hi ESP" (WakeNet)
- version ROBI_VERSION (env, default 2.5.4), higher than the device runs, for OTA
- no subtitles on the round screen (the face says it all); status line stays
- idle screen: Robi's energy / joy bars (from <ota host>/robi/pet) and two child buttons;
  a child button opens his activity menu, and an activity starts a conversation that
  opens with e.g. "זה לביא. בוא נתרגל חשבון" so Robi knows who and what
- tap on the screen: while listening = "I finished", while speaking = stop
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

# 2b. Robi's round-screen UI: pet bars, child buttons, activity menus, tap to stop
#     (C++ lives in scripts/robi_ui.inc and goes into the board's CustomLcdDisplay class)
board_cc = fw / "main/boards/waveshare/esp32-s3-touch-amoled-1.75/esp32-s3-touch-amoled-1.75.cc"
must_replace(board_cc, '#include <lvgl.h>\n',
             '#include <lvgl.h>\n#include <cstring>\n#include <esp_timer.h>\n#include <cJSON.h>\n'
             '#include "assets/lang_config.h"\n')
robi_ui = (Path(__file__).resolve().parent / "robi_ui.inc").read_text(encoding="utf-8")
must_replace(
    board_cc,
    '        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n    }\n};\n',
    '        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);\n'
    '        RobiSetupUI();\n'
    '    }\n' + robi_ui + '};\n',
)

# 3. dark theme by default
must_replace(fw / "main/display/lcd_display.cc",
             'settings.GetString("theme", "light")', 'settings.GetString("theme", "dark")')
# 4. version (must be higher than what the device runs, for OTA)
version = os.environ.get("ROBI_VERSION", "2.6.1")
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
