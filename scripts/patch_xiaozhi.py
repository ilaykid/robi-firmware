"""Patch a xiaozhi-esp32 checkout into Robi firmware.

usage: python patch_xiaozhi.py <xiaozhi_dir> <wake: heyrobby|hiesp>

- Hebrew UI (he-IL locale is chosen with build.py --language)
- LVGL right-to-left text (BIDI)
- Hebrew+Latin text font and Robi's faces in the assets partition
- dark theme by default (Robi's face is drawn for a black AMOLED)
- wake word: custom English "hey robby" (MultiNet) or preset "Hi ESP" (WakeNet)
"""
import json, re, sys
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
found = False
for b in cfg["builds"]:
    if b["name"] == "esp32-s3-touch-amoled-1.75":
        b["sdkconfig_append"] = b.get("sdkconfig_append", []) + extra
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

# 3. dark theme by default
must_replace(fw / "main/display/lcd_display.cc",
             'settings.GetString("theme", "light")', 'settings.GetString("theme", "dark")')
print("done")
