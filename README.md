# Robi firmware

Hebrew firmware for **Robi**, a kids' AI pet built on the Waveshare
ESP32-S3-Touch-AMOLED-1.75 (standard, SKU 31261), based on
[xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.5.2 (MIT).

What it changes (see `scripts/patch_xiaozhi.py`):
- Hebrew UI strings (`--language he-IL`) and right-to-left text (LVGL BIDI)
- Hebrew + Latin text font, `assets/font_noto_sans_common_30_4.bin`
  (Noto Sans + Noto Sans Hebrew, SIL OFL), 30px / 4bpp
- Robi's 21 faces (`assets/faces`, made by `scripts/make_faces.py`)
- Dark theme by default
- Two variants: wake word **"hey robby"** (custom MultiNet) and **"Hi ESP"** (WakeNet)

Every push to `main` builds both variants in GitHub Actions. Download
`robi-firmware-heyrobby` (or `-hiesp`) from the run's Artifacts, unzip, and
flash `merged-binary.bin` at address `0x0` with
https://espressif.github.io/esptool-js/
