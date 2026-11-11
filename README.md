<div dir="rtl">

# HeyRobi 🤖 — רובוט AI קולי לילדים בקוד פתוח (ללא שרת!)

![HeyRobi 3D Enclosure](case/Robi_v4/robi_front.png)

**HeyRobi** הופך לוח `Waveshare ESP32-S3-Touch-AMOLED-1.75` עם מסך מגע עגול ומארז מודפס בתלת־ממד לחבר AI קולי חכם, בטוח ומותאם אישית לילדים — **בחיבור ישיר ל-Google Gemini Live מעל ה-WiFi הביתי, ללא שום צורך בשרת, ב-Docker או במחשב דולק!**

---

## ✨ מה הופך את HeyRobi למיוחד?

- **אפס שרתים (Zero-Server On-Device):** בקר ה-`ESP32-S3` מתחבר ישירות ב-WebSocket מוצפן אל `Google Gemini Live` עם אודיו דו-כיווני חי ב-`24kHz` (ללא קידוד ביניים וללא קטיעות!).
- **אשף התקנה מלא בדפדפן בלחיצה אחת (WebSerial Wizard):** אין צורך להתקין `Python`, `ESP-IDF` או לקמפל קוד. פותחים את אשף ההתקנה ב-Chrome/Edge, בוחרים קול (כולל השמעת דגימה חיה בדפדפן!), בוחרים עיצוב פנים מתוך 5 ערכות ניאון, מגדירים את שמות הילדים ותחומי העניין שלהם, מזינים WiFi ביתי וצורבים בלחיצה אחת בכבל USB-C!
- **ממשק ניהול אלחוטי להורים מהבית (`http://robi.local`):** אחרי ההתקנה הראשונית אין צורך לחבר כבל לעולם — רובי מריץ פורטל ניהול מקומי ברשת הביתית שמאפשר לשנות קול, עיצוב פנים, פרופילי ילדים, עוצמת שמע, בהירות ומפתח API ישירות מהטלפון עם עדכון חי מיידי על המסך.
- **פרופילי ילדים מותאמים אישית (0–3 ילדים):** לכל ילד/ה כפתור צבעוני על המסך העגול שנפתח ל-4 פעילויות מותאמות לגיל (חשבון, אנגלית, ידע כללי, סיפור בהמשכים, חלל, חידות ועוד).
- **זיכרון שיחה מתמשך ובקרות קוליות חכמות:** רובי זוכר עובדות חדשות שהילדים סיפרו לו בזיכרון הפנימי (`NVS`), יודע לשנות לעצמו ווליום ובהירות בפקודה קולית, מזהה רגשות בזמן אמת ומסנכרן את תנועת הפה להברות הדיבור.
- **נעילת שפה קשיחה (עברית / אנגלית):** מניעת מעבר אקראי לשפות זרות מרעשי רקע.

---

## 🛠️ רשימת רכיבים (BOM) להרכבה ביתית

| רכיב | דגם מומלץ | הערות |
|------|-----------|-------|
| לוח פיתוח + מסך מגע עגול | **Waveshare ESP32-S3-Touch-AMOLED-1.75** (`SKU 31261`) | כולל מסך AMOLED עגול `466×466`, זוג מיקרופונים דיגיטליים, מגבר שמע `ES8311` ושבב ניהול סוללה `AXP2101` |
| רמקול | רמקול `8Ω 1W` / `8Ω 2W` עם מחבר `MX1.25 2-Pin` | מגיע לרוב בערכה של Waveshare |
| סוללה נטענת (אופציונלי) | סוללת `LiPo 3.7V` (למשל `603040` / `803040` עד `42×32×10 מ"מ`) עם מחבר `MX1.25` | רובי עובד מצוין גם בחיבור קבוע לכבל USB-C ללא כיבוי אוטומטי |
| מארז מודפס בתלת־ממד | קבצי `STL` + `OpenSCAD` בתיקייה [`case/Robi_v4/`](case/Robi_v4/README.md) | כולל גוף קדמי/אחורי, אוזניות/אנטנה, כפתורי צד, רגליים ומעמד שולחני |

---

## 🚀 התקנה ב-3 דקות (ללא שום ידע בתכנות!)

1. הפיקו מפתח API חינמי ב-[Google AI Studio](https://aistudio.google.com/apikey).
2. פתחו את **אשף ההתקנה של HeyRobi** בדפדפן **Google Chrome** או **Microsoft Edge** במחשב:
   - דרך אתר הפרויקט ב-GitHub Pages: `https://<YOUR-USER>.github.io/hey-robi/`
   - או מקומית מתוך תיקיית הפרויקט: `python -m http.server 8080 -d docs` וגלישה אל `http://localhost:8080`.
3. עברו על 5 השלבים באשף:
   - **שלב 1:** הדביקו את מפתח ה-Gemini ולחצו **בדוק מפתח**.
   - **שלב 2:** בחרו שם לרובוט, שפה וקול (מומלץ: `Puck`) — לחצו **השמע דגימת קול** כדי לשמוע אותו חי!
   - **שלב 3:** בחרו עיצוב פנים מתוך 5 ערכות צבעים חיות (`Cyber Cyan`, `Solar Gold`, `Emerald Matrix`, `Cosmic Violet`, `Coral Cute`).
   - **שלב 4:** הגדירו 0–3 ילדים ו-4 כפתורי פעילות לכל ילד/ה.
   - **שלב 5:** הזינו את שם רשת ה-WiFi הביתית (`2.4GHz`) והסיסמה, חברו את רובי ב-USB-C ולחצו **צריבה מלאה לרובי חדש**!

---

## 📱 ניהול אלחוטי להורים אחרי ההתקנה

מרגע שרובי מחובר ל-WiFi הביתי, כל מכשיר באותה רשת (טלפון או מחשב) יכול לגשת אל:
- `http://robi.local` (או `http://robi` / כתובת ה-IP של רובי ברשת)

ממשק הניהול המקומי מאפשר:
- שינוי קול, שפה וערכת צבעים לפנים (מתעדכן מיד על המסך של רובי!).
- עריכת שמות הילדים, גילאים, תחומי עניין ו-4 כפתורי הפעילות של כל ילד/ה.
- שליטה חיה בווליום ובבהירות וניקוי זיכרון השיחה של רובי.
- החלפת מפתח `Gemini API` בכל עת.

אם החלפתם נתב או לקחתם את רובי לבית אחר שבו ה-WiFi לא מוכר, רובי יפתח אוטומטית נקודת גישה אלחוטית בשם `Robi-XXXX` להגדרת רשת חדשה מהטלפון.

---

## 🧩 מבנה המאגר ופיתוח מקוד מקור

```text
hey-robi/
├── docs/                        # אשף ההתקנה האינטראקטיבי בדפדפן + צורב WebSerial (GitHub Pages)
├── firmware/robi/               # מודולי הליבה העצמאיים של HeyRobi ב-C++:
│   ├── robi_config.{h,cc}       # ניהול הגדרות NVS + ייבוא ROBICFG1 מכתובת 0x010000 + זיכרון שיחה
│   ├── robi_gemini_live.{h,cc}  # לקוח WebSocket ישיר מול Google Gemini Live + באפר PSRAM 512KB
│   └── robi_http_portal.{h,cc}  # פורטל ניהול להורים בפורט 80 + משיב mDNS עבור robi.local
├── scripts/
│   ├── patch_xiaozhi.py         # סקריפט הזרקת השינויים מעל xiaozhi-esp32 v2.5.2
│   └── robi_ui.inc              # מנוע הפנים Cyber-Optic LVGL עם 5 ערכות עיצוב וכפתורי ילדים דינמיים
├── assets/                      # גופן Noto Sans Hebrew + סמלי הבעות
├── case/Robi_v4/                # קבצי הדפסה תלת-ממדית (STL + OpenSCAD) והוראות הרכבה
└── .github/workflows/build.yml  # בניית קושחה אוטומטית (ESP-IDF v6.1) + פריסת אשף ההתקנה ל-GitHub Pages
```

### קימפול עצמאי מקוד מקור (אופציונלי למפתחים)

המאגר כולל תהליך GitHub Actions אוטומטי ב-[`.github/workflows/build.yml`](.github/workflows/build.yml) שמייצר את קבצי ה-`.bin` המוכנים (`robi-full-heyrobby.bin` ו-`robi-full-hiesp.bin`) ומעלה אותם אוטומטית עם אשף ההתקנה ל-GitHub Pages בכל דחיפה ל-`main`.

לבנייה מקומית באמצעות Docker (`espressif/idf:v6.1`):

```bash
git clone --depth 1 --branch v2.5.2 https://github.com/78/xiaozhi-esp32 fw
python3 scripts/patch_xiaozhi.py fw heyrobby
docker run --rm -v "$PWD:/work" -w /work/fw \
  -e ROBI_TEXT_FONT=/work/assets/font_noto_sans_common_30_4.bin \
  -e ROBI_EMOJI_DIR=/work/assets/faces \
  espressif/idf:v6.1 bash -lc '
    source $IDF_PATH/export.sh &&
    python scripts/build.py waveshare/esp32-s3-touch-amoled-1.75 \
      --name esp32-s3-touch-amoled-1.75 --language he-IL
  '
```

---

## 📄 רישיון וקרדיטים

מופץ תחת רישיון **MIT** (ראו [`LICENSE`](LICENSE)).  
תשתית הדרייברים והאודיו מבוססת בהערכה רבה על פרויקט הקוד הפתוח המצוין [`78/xiaozhi-esp32`](https://github.com/78/xiaozhi-esp32) (רישיון MIT).

</div>

---

## 🌍 English Overview

**HeyRobi** is a 100% standalone, **zero-server** open-source AI companion robot for kids built around the **Waveshare ESP32-S3-Touch-AMOLED-1.75** (`466×466` round touch AMOLED) and a 3D-printable desktop body (`case/Robi_v4/`).

- **Direct ESP32-S3 → Google Gemini Live WebSocket:** Streams `16kHz` mic PCM up and plays `24kHz` native Gemini audio down through a `512KB` PSRAM jitter ring buffer — no Python backend, no cloud relay, no PC required.
- **1-Click Browser Setup & WebSerial Installer (`docs/`):** Configure your `GEMINI_API_KEY`, preview voices live in the browser, pick from **5 cyber-optic face color themes**, set up `0–3` child profiles with custom activity buttons, enter `2.4GHz` WiFi credentials, and flash everything over USB-C directly from Chrome/Edge.
- **Built-in Wireless Parent Portal (`http://robi.local`):** Update voice, face theme, kids, volume, brightness, or API key anytime over home WiFi with instant live UI reload.
