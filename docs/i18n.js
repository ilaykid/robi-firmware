// Bilingual UI dictionary for HeyRobi WebSerial Setup Wizard (English default + Hebrew)

export const I18N = {
  en: {
    dir: "ltr",
    langSwitchAria: "Interface language",
    pageTitle: "HeyRobi — Browser Setup & Customization Wizard",
    headerTitle: "HeyRobi — Browser Setup & Customization Wizard",
    headerSubtitle: "Open-source AI voice robot for kids • Direct to Gemini Live • Zero server • No software install",
    privacyBadge: "🔒 100% Local in Browser • Zero Data Sent to Any Server",
    btnExportProfile: "Export Profile",
    btnImportProfile: "Load Profile",
    privacyBannerAria: "Privacy guarantee and zero data storage",
    privacyBannerHtml:
      '<strong style="color:#34d399;font-size:0.94rem;">Total Privacy Guarantee — Zero data is sent to us, and nothing is ever stored by us!</strong><br />' +
      "This page is a static, client-side browser tool (<code>Client-Side WebSerial</code> with no backend server and no database). " +
      '<strong>Your Gemini API key, WiFi credentials, and kids\' names are never sent to the project authors or this site, never pass through any third-party server, and are not even saved in your browser\'s <code>localStorage</code></strong> — they are flashed directly from your browser over USB-C into your HeyRobi\'s internal flash chip at home.',

    stepperAria: "Setup steps",
    step1Label: "Gemini Key",
    step2Label: "Name & Voice",
    step3Label: "Face Design",
    step4Label: "Topics & Kids",
    step5Label: "WiFi & Flash",

    // Step 1
    s1Title: "Step 1 — Direct Connection to Google Gemini",
    s1Sub: "Your robot connects directly over home WiFi to Google Gemini Live — with zero relay server in between! Your API key is stored exclusively inside HeyRobi's local memory at home.",
    s1GuideTitle: "How to get a free API key in 30 seconds:",
    s1GuideStep1Html: 'Open <a href="https://aistudio.google.com/apikey" target="_blank" rel="noopener">Google AI Studio — API Keys</a> and sign in with your Google account.',
    s1GuideStep2Html: "Click <strong>Create API key</strong> and copy the key starting with <code>AIza...</code>.",
    s1GuideStep3Html: "Paste it below and click <strong>Verify Key</strong> to confirm it works.",
    s1KeyLabel: "Google Gemini API Key",
    btnVerifyKey: "Verify Key",
    s1KeyNoteHtml: "🔒 <strong>Zero tracking &amp; zero storage:</strong> Your key is never sent to this site or stored in your browser — it is verified directly against Google and flashed over USB-C straight into your HeyRobi.",
    s1ModelLabel: "Gemini Live Model for Real-Time Voice",
    optModelAuto: "Smart Auto — Gemini 3.8 Live Primary + Auto Failover (Recommended!)",
    optModel38: "Gemini 3.8 Live — Next-Gen Multilingual Native Voice",
    optModel25: "Gemini 2.5 Flash Native Audio — Fast & Reliable",
    optModel20: "Gemini 2.0 Flash Live — Versatile Fallback Model",
    s1ModelNoteHtml: "Google's Free Tier includes roughly ~45 minutes of active net voice talk time per day (up to 500 sessions / 1,000,000 tokens, resetting daily at Google). You can monitor estimated daily usage in HeyRobi's local Parent Portal (<code>http://robi.local</code>).",
    navNextVoice: "Next: Name & Voice →",

    // Step 2
    s2Title: "Step 2 — Robot Name, Language & Voice",
    s2Sub: "Pick your robot companion's name, spoken language, and voice — you can preview the voice live right here!",
    s2RobotNameLabel: "Robot Name",
    s2LanguageLabel: "Spoken Language (Language Lock)",
    optLangEn: "English Only (en-US)",
    optLangHe: "עברית בלבד — Hebrew Only (he-IL)",
    s2VoiceLabel: "Choose HeyRobi's Voice",
    btnPlayVoice: "🔊 Play Live Voice Sample (with Lip-Sync!)",
    navPrev: "← Back",
    navNextFace: "Next: Face Design →",

    // Step 3
    s3Title: "Step 3 — Robot Face Style & Neon Color Theme",
    s3SubHtml: "Pick both the <strong>robot face geometry</strong> and the <strong>neon color palette</strong> (25 unique combinations!). Every click updates the live simulator.",
    s3StyleHeading: "1. Choose a Robot Face Style:",
    s3ThemeHeading: "2. Choose a Neon Color Theme:",
    s3VolumeLabel: "Initial Volume:",
    s3BrightnessLabel: "AMOLED Screen Brightness:",
    navNextTopics: "Next: Topics & Kids →",

    // Step 4
    s4Title: "Step 4 — Mode (Kids / Assistant / Hybrid), Topics & Profiles",
    s4Sub: "Choose whether HeyRobi runs as a kids companion & arcade, a smart desktop personal assistant (with live Google Search), or hybrid mode for the whole family!",
    s4PersonaLabel: "HeyRobi Operating Mode",
    optPersonaHybrid: "Family Hybrid — Tap face = Adult Personal Assistant, Tap kid button = Kids Mode (Recommended!)",
    optPersonaKids: "Interactive Kids Companion & Games Only",
    optPersonaAssistant: "Desktop Personal Assistant Only (no kid buttons on screen)",
    s4PersonaNoteHtml: "💡 In every mode, swiping sideways on HeyRobi's round screen opens the touch &amp; motion arcade (including offline <strong>Heads Up!</strong> forehead charades and live <strong>Screen Trivia</strong>)!",
    s4TopicsLabel: "💬 Add Your Own Conversation Topics (Parent Free-Text)",
    s4TopicsSub: "Type any topic your kids love or you'd like HeyRobi to encourage — basketball, chess, first-grade prep, Greek mythology, Minecraft, music, science curiosity, and more:",
    s4TopicsPlaceholder: "Type a custom topic and press Enter or click Add...",
    btnAddTopic: "+ Add Topic",
    s4TopicsEmpty: "No custom topics added yet — type any topic above to add it!",
    s4RemoveTopicTitle: "Remove topic",
    s4MaxTopicsAlert: "You can add up to 16 custom conversation topics.",
    btnAddKid: "+ Add Kid Profile (up to 3)",
    s4PromptLabel: "Special Parent Instructions for HeyRobi (Optional)",
    s4PromptPlaceholder: "e.g. Encourage curiosity and independent thinking, keep sentences short and clear, add gentle humor...",
    navNextFlash: "Next: WiFi & Flash →",

    // Kid box
    kidHeader: (n) => `Kid Profile #${n}`,
    kidRemove: "Remove",
    kidNameLabel: "Child's Name (shown on the round screen)",
    kidAgeLabel: "Age",
    kidColorLabel: "Button Color",
    kidInterestsLabel: "Interests & Hobbies (free text — HeyRobi weaves these into chats!)",
    kidInterestsPlaceholder: "e.g. Lego, soccer, space, drawing, chess, Minecraft...",
    kidActivitiesLabel: (name) => `4 Activity Buttons on ${name}'s Screen (add your own or quick-fill from presets):`,
    kidCustomActTitle: "✨ Add a custom activity button to the screen:",
    kidCustomActLabelPh: "Button label",
    kidCustomActPromptPh: "What should HeyRobi do? (e.g. Ask me a world soccer trivia quiz!)",
    kidCustomActBtn: "+ Add to Slot",
    kidPresetsHint: "Or quick-fill from a ready-made preset (you can freely edit every row below):",
    kidActLabelPh: (n) => `Button #${n} label`,
    kidActPromptPh: "What should HeyRobi talk about when tapped?",
    kidFallbackName: (n) => `Kid ${n}`,
    kidFallbackAct: "Activity",
    kidDefaultPrompt: (lbl) => `Let's talk and play about ${lbl}!`,

    // Step 5
    s5Title: "Step 5 — Home WiFi & 1-Click USB-C Flash!",
    s5SubHtml:
      "Connect HeyRobi to your computer with a USB-C cable. One click flashes HeyRobi's firmware and your personal settings directly to the chip!" +
      '<br /><strong style="color:#34d399;">🔒 Your WiFi credentials live in your browser RAM only — they are never sent to this site or saved anywhere outside your HeyRobi.</strong>',
    s5SsidLabel: "Home WiFi Network Name (2.4GHz band)",
    s5SsidNoteHtml: "Note: ESP32-S3 connects to 2.4GHz WiFi networks (leave blank if you prefer HeyRobi to open a <code>Robi-XXXX</code> hotspot for phone setup).",
    s5PassLabel: "WiFi Password",
    s5BoardLabel: "Target Hardware / Board Model",
    optBoardWaveshare: 'Waveshare 1.75" AMOLED — Primary Tested Hardware (Recommended)',
    optBoardSpotpear: 'Spotpear 1.28" BOX — Budget Alternative (Experimental — Untested on Hardware)',
    s5SpotpearNoteHtml:
      '🧪 <strong>Experimental Notice:</strong> Support for <code>Spotpear 1.28" BOX</code> (<code>240×240</code> IPS screen with built-in enclosure &amp; battery) was added to lower hardware costs, but has <strong>not yet been verified on physical hardware</strong>. If you flash one, please share feedback on <a href="https://github.com/ilaykid/robi-firmware/issues" target="_blank" rel="noopener" style="color:#fde68a;text-decoration:underline;">GitHub Issues</a>!',
    s5WakeLabel: "Voice Wake Word",
    optWakeRobi: '"Hey Robby" — Dedicated HeyRobi Wake Word (Recommended)',
    optWakeEsp: '"Hi ESP" — Alternative Built-in Wake Model',
    btnFlashFull: "🚀 Full Flash for New Robi (Firmware + Personal Settings over USB)",
    btnFlashUpgrade: "🔄 Upgrade Existing Robi Firmware (Keeps All Memories, WiFi & API Key!)",
    btnFlashConfig: "⚡ Update Settings Only over USB (Already Flashed — 1 Second!)",
    s5AdvSummary: "Advanced Options / Wireless Settings from Home",
    s5AdvWirelessHtml:
      "💡 <strong>Did you know?</strong> After the first flash you never need a USB cable again! Open " +
      "<code>http://robi.local</code> (or <code>http://robi</code>) on your home WiFi anytime to update face style, colors, voice, topics, or kids wirelessly from your phone!",
    s5AdvBootHtml:
      "<strong>Tip if your PC doesn't detect HeyRobi:</strong> Hold the small <code>BOOT</code> button on the board while plugging in USB-C, then release after 1 second.",
    s5AdvLocalBinHtml: "Load a local firmware (<code>.bin</code>) file from your computer (optional for developers):",

    // Preview & Hardware sidebar
    emoHappy: "Happy",
    emoLaughing: "Laughing",
    emoSurprised: "Surprised",
    emoWinking: "Winking",
    emoCool: "Cool",
    emoThinking: "Thinking",
    emoSpeaking: "Speaking",
    simCaption: "Live preview of the round AMOLED display (466×466) • Click the kid buttons inside the screen to preview the activity menu!",
    hwTitle: "🛠️ Building a New HeyRobi? Parts & Enclosure",
    hwSub: "Assemble HeyRobi yourself at home in 10 minutes with zero soldering:",
    hwWaveshareHtml: '🔌 <strong>Waveshare 1.75" AMOLED Board</strong><br /><span style="font-size:0.77rem;font-weight:400;color:var(--text-muted);">Search on AliExpress (bundle with speaker &amp; battery recommended)</span>',
    hwBuyCta: "Shop ↗",
    hwWaveshareTipHtml: '💡 <strong>Buying tip:</strong> Opens search for the exact model (<code>ESP32-S3-Touch-AMOLED-1.75</code>). Pick the bare board (no metal case) bundled with a <strong>speaker &amp; 3.7V LiPo battery</strong> so it fits right into the HeyRobi 3D case!',
    hwSpotpearHtml: '💸 <strong>Spotpear 1.28" BOX (Budget — Experimental)</strong><br /><span style="font-size:0.76rem;font-weight:400;color:var(--text-muted);">Includes round touch screen, speaker, mic, battery &amp; case (~$20). <strong>Untested!</strong></span>',
    hwSpotpearTipHtml: '🧪 If trying Spotpear on a budget: select the <strong>With Touch &amp; Battery</strong> option, then choose <strong>Spotpear 1.28" BOX (Experimental)</strong> in Step 5. Feedback welcome!',
    hwStlBtn: "🖨️ 3D Case / STL Files",
    hwGuideBtn: "📖 Full Guide (GitHub)",

    // Dynamic status & alert messages
    stKeyInvalid: "Please enter a valid Gemini API key (usually starts with AIza).",
    stKeyChecking: "Verifying API key with Google Gemini...",
    stKeyRejected: (msg) => `API key rejected: ${msg}`,
    stKeyOk: "Awesome! Your API key is valid and active with Google Gemini.",
    stKeyNetErr: (msg) => `Network error while verifying key: ${msg}`,
    stVoiceNeedKey: "To play a live voice sample, please enter your Gemini API key in Step 1 first.",
    stVoiceLoading: (v) => `Generating live voice sample for ${v}...`,
    stVoiceNoAudio: "No audio returned from Gemini TTS",
    stVoicePlaying: (v) => `Now playing ${v} — watch HeyRobi's animated face!`,
    stVoiceDone: (v) => `Voice sample (${v}) played successfully!`,
    stVoiceErr: (msg) => `Voice sample error: ${msg}`,
    stCfgTooLarge: (n) => `Configuration JSON exceeds maximum allowed size (${n} bytes)`,
    stFwMissingSpotpear: (url, status) =>
      `Experimental Spotpear 1.28" firmware (${url}) is not hosted here yet (HTTP ${status}). Select Waveshare 1.75" or load a local .bin file below.`,
    stFwMissing: (url, status) =>
      `Firmware binary (${url}) was not found on this server (HTTP ${status}). Select a local .bin file below or use the official GitHub Pages wizard.`,
    stNoWebSerial: "Your current browser does not support WebSerial. Please open this wizard in Google Chrome or Microsoft Edge on a desktop computer.",
    stNoKeyConfirm: "You haven't entered a Gemini API key in Step 1. HeyRobi will boot up but will ask you to configure a key at http://robi.local before it can talk. Continue flashing?",
    stDlUpgrade: "Downloading HeyRobi firmware update (preserving all memories & settings)...",
    stDlFull: "Downloading HeyRobi firmware image...",
    stBadBin: "The firmware file is not a valid ESP32 image (missing 0xE9 magic header).",
    stPickPort: "Select your HeyRobi in the USB popup (USB JTAG/serial debug unit)...",
    stConnecting: "Connecting to HeyRobi's ESP32-S3 chip...",
    stFlashingCfg: "Updating settings in HeyRobi's memory (1 second)...",
    stFlashingUpg: "Upgrading firmware only while keeping all memories & settings (~15–25s, do not unplug)...",
    stFlashingFull: "Flashing HeyRobi firmware and your personal settings (~45–90s, do not unplug cable)...",
    stFlashOkUpg: "Upgrade complete! All your memories, WiFi, and settings were preserved 🎉",
    stFlashOkFull: "Flash complete! HeyRobi is waking up and connecting to your home WiFi 🎉",
    stFlashErr: (msg) => `Flashing error: ${msg}`,
    stProfileLoaded: "Profile loaded successfully!",
    stProfileInvalid: (msg) => `Invalid JSON profile file: ${msg}`,
    stCustomBinSelected: (name) => `Selected local firmware file: ${name}`,

    // Defaults
    defaultRobotName: "Robi",
    defaultLanguage: "en-US",
    defaultKidName: "Maya",
    defaultKidInterests: "space, animals, riddles and stories",
    newKidInterests: "games, riddles and stories",

    // Voices
    voices: [
      { id: "Puck", label: "Puck ⭐ Recommended", desc: "Upbeat, playful, energetic, and super natural for kids" },
      { id: "Zephyr", label: "Zephyr", desc: "Bright, breezy, friendly, and light" },
      { id: "Leda", label: "Leda", desc: "Youthful, warm, and curious" },
      { id: "Aoede", label: "Aoede", desc: "Rich, melodic, and a wonderful storyteller" },
      { id: "Fenrir", label: "Fenrir", desc: "Enthusiastic, adventurous, and full of energy" },
      { id: "Achird", label: "Achird", desc: "Calm, companionable, and gentle on the ears" },
      { id: "Kore", label: "Kore", desc: "Clear, patient, and great for learning practice" },
      { id: "Charon", label: "Charon", desc: "Steady, wise, and knowledgeable" },
    ],

    // Activity presets
    presets: [
      { label: "Math Fun", prompt: "Let's practice math in a fun way! Ask me age-appropriate puzzles and exercises and cheer me on." },
      { label: "Word Games", prompt: "Let's play fun vocabulary and rhyming word games! Teach me cool new words and quiz me." },
      { label: "Trivia Quiz", prompt: "Let's play a general knowledge trivia game about science, nature, animals, and the world!" },
      { label: "Adventure Story", prompt: "Let's invent an exciting choose-your-own-adventure story where I decide what the hero does next!" },
      { label: "Space & Science", prompt: "Tell me a mind-blowing fact about space, planets, or inventions and ask me what I think!" },
      { label: "Logic Riddles", prompt: "Give me a short detective mystery or logic riddle and give me hints until I solve it!" },
      { label: "Dinosaurs", prompt: "Let's explore dinosaurs and rare wild animals with a fun true-or-false quiz!" },
    ],

    // Face Styles & Themes
    faceStyles: {
      cyber_visor: { title: "Cyber Capsules", subtitle: "Vertical glass eyes with laser scanline" },
      round_kawaii: { title: "Round Kawaii", subtitle: "Big sparkling pupils with rosy cheeks" },
      retro_pixel: { title: "Retro Pixel", subtitle: "8-bit square arcade robot eyes" },
      visor_cyclops: { title: "Cyborg Visor", subtitle: "Wide helmet visor with sweeping scanner beam" },
      owl_mech: { title: "Mech Lenses", subtitle: "Dual mechanical aperture rings" },
    },
    faceThemes: {
      cyber_cyan: { title: "Cyber Cyan", subtitle: "Classic glowing neon cyan" },
      solar_gold: { title: "Solar Gold", subtitle: "Warm, sunny, and energetic" },
      emerald_matrix: { title: "Emerald Matrix", subtitle: "Crisp sci-fi neon green" },
      cosmic_violet: { title: "Cosmic Violet", subtitle: "Deep space galactic shimmer" },
      coral_cute: { title: "Coral Cute", subtitle: "Friendly peach-pink glow" },
    },

    // Simulator canvas text
    simGreeting: (name) => `Hi, I'm ${name}!`,
    simKidMenuTitle: (name) => `${name} — What's next?`,
    simBackToFace: "Back to Face",
    simKidFallback: (i) => `Kid ${i}`,
    simActFallback: (i) => `Activity ${i}`,
  },

  he: {
    dir: "rtl",
    langSwitchAria: "שפת ממשק",
    pageTitle: "HeyRobi — אשף התקנה ועיצוב אישי בדפדפן",
    headerTitle: "HeyRobi — אשף התקנה ועיצוב אישי",
    headerSubtitle: "רובוט AI קולי לילדים בחיבור ישיר ל-Gemini Live • ללא שרת • ללא התקנת תוכנה",
    privacyBadge: "🔒 100% מקומי בדפדפן • אפס מידע נשלח לשרת",
    btnExportProfile: "ייצוא פרופיל",
    btnImportProfile: "טעינת פרופיל",
    privacyBannerAria: "הצהרת פרטיות ואפס אחסון מידע",
    privacyBannerHtml:
      '<strong style="color:#34d399;font-size:0.94rem;">התחייבות לפרטיות מוחלטת — שום מידע לא נשלח אלינו וקל וחומר לא מאוחסן אצלנו!</strong><br />' +
      "עמוד זה הוא כלי דפדפן סטטי ומקומי בלבד (<code>Client-Side WebSerial</code> ללא שרת אחורי וללא מסד נתונים). " +
      '<strong>מפתח ה-Gemini שלכם, פרטי ה-WiFi ושמות הילדים לעולם אינם נשלחים למפתחי הפרויקט או לאתר זה, אינם עוברים דרך שום שרת צד-שלישי, ואפילו אינם נשמרים ב-<code>localStorage</code> של הדפדפן שלכם</strong> — הם נכתבים ישירות מתוך הדפדפן דרך כבל ה-USB אל שבב הזיכרון הפנימי של רובי אצלכם בבית בלבד.',

    stepperAria: "שלבי התקנה",
    step1Label: "מפתח Gemini",
    step2Label: "שם וקול",
    step3Label: "עיצוב פרצוף",
    step4Label: "נושאים וילדים",
    step5Label: "WiFi וצריבה",

    // Step 1
    s1Title: "שלב 1 — חיבור ישיר ל-Google Gemini",
    s1Sub: "הרובוט מתחבר בעצמו דרך ה-WiFi הביתי ישירות ל-Google Gemini Live — בלי שום שרת באמצע! המפתח נשמר אך ורק בזיכרון הפנימי של רובי אצלכם בבית.",
    s1GuideTitle: "איך משיגים מפתח API תוך 30 שניות?",
    s1GuideStep1Html: 'נכנסים אל <a href="https://aistudio.google.com/apikey" target="_blank" rel="noopener">Google AI Studio — API Keys</a> ומתחברים עם חשבון Google.',
    s1GuideStep2Html: "לוחצים על <strong>Create API key</strong> ומעתיקים את המפתח שמתחיל ב-<code>AIza...</code>.",
    s1GuideStep3Html: "מדביקים כאן למטה ולוחצים על <strong>בדוק מפתח</strong> כדי לוודא שהוא פעיל.",
    s1KeyLabel: "מפתח Google Gemini API",
    btnVerifyKey: "בדוק מפתח",
    s1KeyNoteHtml: "🔒 <strong>אפס שליחה ואפס אחסון:</strong> המפתח לעולם לא נשלח למפתחי האתר ולא נשמר בדפדפן — הוא נבדק ישירות מול Google ונצרב בכבל ה-USB לתוך רובי אצלכם בבית בלבד.",
    s1ModelLabel: "מודל Gemini Live לשיחה קולית בזמן אמת",
    optModelAuto: "אוטומטי חכם — Gemini 3.8 Live ראשי + גיבוי אוטומטי (מומלץ!)",
    optModel38: "Gemini 3.8 Live — הדור החדש עם עברית מובנית (he-IL)",
    optModel25: "Gemini 2.5 Flash Native Audio — מהיר ויציב",
    optModel20: "Gemini 2.0 Flash Live — מודל גיבוי רב-תכליתי",
    s1ModelNoteHtml: "במסלול החינמי של Google (Free Tier) יש מכסה של כ-45 דקות שיחה נטו ביום (עד 500 סשנים / 1,000,000 טוקנים שמתאפסים אוטומטית אצל Google). ניתן לראות מד מעקב מקומי משוער בפורטל הביתי של רובי (<code>http://robi.local</code>).",
    navNextVoice: "המשך לבחירת קול ←",

    // Step 2
    s2Title: "שלב 2 — שם הרובוט, שפה ובחירת קול",
    s2Sub: "בחרו איך יקראו לחבר הרובוטי שלכם, באיזו שפה הוא ידבר, ואיזה קול הכי מתאים לו — אפשר לשמוע דגימת קול חיה ממש כאן!",
    s2RobotNameLabel: "שם הרובוט",
    s2LanguageLabel: "שפת שיחה קבועה (נעילת שפה)",
    optLangEn: "English Only (en-US)",
    optLangHe: "עברית בלבד (he-IL)",
    s2VoiceLabel: "בחרו את הקול של רובי",
    btnPlayVoice: "🔊 השמע דגימת קול חיה (עם סינכרון שפתיים!)",
    navPrev: "→ הקודם",
    navNextFace: "המשך לעיצוב פרצוף ←",

    // Step 3
    s3Title: "שלב 3 — סגנון פרצוף רובוטי וצבעי ניאון",
    s3SubHtml: "בחרו גם את <strong>צורת הפנים הרובוטית</strong> וגם את <strong>צבע הניאון</strong> (25 שילובים שונים!). כל לחיצה מתעדכנת מיד בסימולטור משמאל.",
    s3StyleHeading: "1. בחרו סגנון פרצוף רובוטי:",
    s3ThemeHeading: "2. בחרו ערכת צבעי ניאון:",
    s3VolumeLabel: "עוצמת שמע התחלתית:",
    s3BrightnessLabel: "בהירות מסך AMOLED:",
    navNextTopics: "המשך לנושאים וילדים ←",

    // Step 4
    s4Title: "שלב 4 — מצב הפעלה (ילדים / עוזר אישי / משולב) ופרופילים",
    s4Sub: "בחרו האם רובי ישמש כחבר ומשחקייה לילדים, כעוזר אישי שולחני חכם למבוגר (עם חיפוש חי בגוגל), או במצב משולב לשניהם יחד!",
    s4PersonaLabel: "מצב הפעלה של רובי",
    optPersonaHybrid: "משולב למשפחה — לחיצה על הפנים = עוזר אישי למבוגר, לחיצה על שם ילד = מצב ילדים (מומלץ!)",
    optPersonaKids: "חבר ומשחקייה אינטראקטיבית לילדים בלבד",
    optPersonaAssistant: "עוזר אישי שולחני למבוגר בלבד (ללא כפתורי ילדים על המסך)",
    s4PersonaNoteHtml: "💡 בכל המצבים החלקה הצידה על המסך העגול של רובי פותחת את משחקיית המגע וחיישן התנועה (כולל <strong>נחש מה על המצח</strong> שעובד גם ללא אינטרנט, ו<strong>טריוויה חיה על המסך</strong>)!",
    s4TopicsLabel: "💬 הוסיפו נושאי שיחה משלכם (כתיבה חופשית של ההורה)",
    s4TopicsSub: "כתבו כל נושא שמעניין את הילדים או שחשוב לכם שרובי יעודד — כדורסל, שחמט, הכנה לכיתה א', מיתולוגיה יוונית, חגי ישראל, מיינקראפט, נגינה, סקרנות מדעית ועוד:",
    s4TopicsPlaceholder: "הקלידו נושא שיחה משלכם ולחצו Enter או הוסף...",
    btnAddTopic: "+ הוסף נושא",
    s4TopicsEmpty: "עדיין לא נוספו נושאים אישיים — הקלידו למעלה כל נושא שתרצו!",
    s4RemoveTopicTitle: "הסרת נושא",
    s4MaxTopicsAlert: "ניתן להוסיף עד 16 נושאי שיחה אישיים.",
    btnAddKid: "+ הוספת ילד/ה (עד 3)",
    s4PromptLabel: "הנחיות מיוחדות מההורים לרובי (אופציונלי)",
    s4PromptPlaceholder: "למשל: לעודד חשיבה עצמאית וסקרנות, לדבר במשפטים קצרים וברורים, לשלב הומור עדין...",
    navNextFlash: "המשך ל-WiFi וצריבה ←",

    // Kid box
    kidHeader: (n) => `פרופיל ילד/ה #${n}`,
    kidRemove: "הסרה",
    kidNameLabel: "שם הילד/ה (מופיע על המסך העגול)",
    kidAgeLabel: "גיל",
    kidColorLabel: "צבע כפתור",
    kidInterestsLabel: "תחומי עניין ותחביבים (כתיבה חופשית — רובי ישלב אותם בשיחות!)",
    kidInterestsPlaceholder: "למשל: לגו, כדורגל, חלל, ציור, שחמט, מיינקראפט...",
    kidActivitiesLabel: (name) => `4 כפתורי נושאים על המסך של ${name} (כתבו כפתור אישי משלכם או בחרו מתבנית):`,
    kidCustomActTitle: "✨ הוספת כפתור נושא אישי משלכם למסך:",
    kidCustomActLabelPh: "שם על הכפתור",
    kidCustomActPromptPh: "מה תרצו שרובי יעשה? (למשל: שאל אותי חידון על כדורגל עולמי!)",
    kidCustomActBtn: "+ הכנס לכפתור",
    kidPresetsHint: "או מילוי מהיר מתבנית מוכנה (ואפשר לערוך חופשי כל שורה למטה):",
    kidActLabelPh: (n) => `כותרת כפתור #${n}`,
    kidActPromptPh: "על מה רובי ידבר כשלוחצים על הכפתור?",
    kidFallbackName: (n) => `ילד/ה #${n}`,
    kidFallbackAct: "פעילות",
    kidDefaultPrompt: (lbl) => `בוא נדבר ונשחק בנושא ${lbl}!`,

    // Step 5
    s5Title: "שלב 5 — חיבור ל-WiFi הביתי וצריבה בלחיצה אחת!",
    s5SubHtml:
      "חברו את רובי למחשב עם כבל USB-C. בלחיצה אחת הדפדפן יצרוב ישירות לשבב את מערכת HeyRobi ואת ההגדרות שלכם!" +
      '<br /><strong style="color:#34d399;">🔒 פרטי ה-WiFi שלכם מעובדים בזיכרון ה-RAM של הדפדפן שלכם בלבד — הם אינם נשלחים לאתר זה ואינם נשמרים בשום מקום מחוץ לרובי.</strong>',
    s5SsidLabel: "שם רשת ה-WiFi הביתית (תדר 2.4GHz)",
    s5SsidNoteHtml: "שימו לב: בקר ESP32-S3 מתחבר לרשתות 2.4GHz (אם תשאירו ריק, רובי יפתח נקודת WiFi בשם <code>Robi-XXXX</code> להגדרה מהטלפון).",
    s5PassLabel: "סיסמת WiFi",
    s5BoardLabel: "דגם הלוח / החומרה לצריבה",
    optBoardWaveshare: 'Waveshare 1.75" AMOLED — החומרה הראשית והבדוקה (מומלץ)',
    optBoardSpotpear: 'Spotpear 1.28" BOX — חלופה זולה להורדת עלויות (ניסיוני — טרם נוסתה בפועל)',
    s5SpotpearNoteHtml:
      '🧪 <strong>גילוי נאות — גרסה ניסיונית:</strong> תמיכה בלוח <code>Spotpear 1.28" BOX</code> (מסך IPS <code>240×240</code>, כולל מארז וסוללה מובנים) נוספה לקוד ולבנייה האוטומטית כדי להוריד עלויות, אך <strong>טרם נוסתה בפועל על חומרה פיזית</strong>. אם הזמנתם וצרבתם — נשמח מאוד לביקורת ומשוב ב-<a href="https://github.com/ilaykid/robi-firmware/issues" target="_blank" rel="noopener" style="color:#fde68a;text-decoration:underline;">GitHub</a>!',
    s5WakeLabel: "מילת הקריאה הקולית שמעירה את רובי",
    optWakeRobi: '"Hey Robby" — מילת הקריאה הייעודית של HeyRobi (מומלץ)',
    optWakeEsp: '"Hi ESP" — מודל קריאה חלופי מובנה',
    btnFlashFull: "🚀 צריבה מלאה לרובי חדש (מערכת + הגדרות אישיות ב-USB)",
    btnFlashUpgrade: "🔄 שדרוג גרסה לרובי קיים (מעדכן קושחה ושומר על כל הזיכרונות, ה-WiFi והמפתח!)",
    btnFlashConfig: "⚡ עדכון הגדרות בלבד ב-USB (לרובי שכבר מותקן — שנייה אחת!)",
    s5AdvSummary: "אפשרויות מתקדמות / שינוי הגדרות אלחוטי מהבית",
    s5AdvWirelessHtml:
      "💡 <strong>ידעתם?</strong> אחרי ההתקנה הראשונית לא צריך יותר לחבר כבל USB! בכל רגע אפשר לפתוח בדפדפן בבית את הכתובת " +
      "<code>http://robi.local</code> (או <code>http://robi</code>) ולשנות סגנון פרצוף, צבעים, קול, נושאים או ילדים אלחוטית מהטלפון!",
    s5AdvBootHtml:
      "<strong>טיפ אם המחשב לא מזהה את רובי:</strong> החזיקו את כפתור ה-<code>BOOT</code> הקטן על הלוח לחוץ בזמן חיבור כבל ה-USB-C, ושחררו אחרי שנייה.",
    s5AdvLocalBinHtml: "בחירת קובץ קושחה (<code>.bin</code>) מקומי מהמחשב (אופציונלי למפתחים):",

    // Preview & Hardware sidebar
    emoHappy: "שמח",
    emoLaughing: "צוחק",
    emoSurprised: "מופתע",
    emoWinking: "קורץ",
    emoCool: "מגניב",
    emoThinking: "חושב",
    emoSpeaking: "מדבר",
    simCaption: "תצוגה חיה של מסך ה-AMOLED העגול (466×466) • לחצו על כפתורי הילדים בתוך המסך כדי לבדוק את תפריט הנושאים!",
    hwTitle: "🛠️ בונים רובי חדש? רשימת החלקים והמארז",
    hwSub: "אפשר להרכיב את רובי לבד בבית תוך דקות (ללא הלחמות!), או להדפיס את המארז:",
    hwWaveshareHtml: '🔌 <strong>לוח Waveshare 1.75" AMOLED</strong><br /><span style="font-size:0.77rem;font-weight:400;color:var(--text-muted);">חיפוש המוצר ב-AliExpress (מומלץ עם רמקול וסוללה)</span>',
    hwBuyCta: "לרכישה ↗",
    hwWaveshareTipHtml: '💡 <strong>טיפ לרכישה:</strong> הקישור פותח חיפוש של הדגם המדויק (<code>ESP32-S3-Touch-AMOLED-1.75</code>). מומלץ לבחור במוכר שמציע את הלוח החשוף (ללא קופסת מתכת) בבאנדל עם <strong>רמקול וסוללת LiPo 3.7V</strong> כדי שייכנס בול למארז של רובי!',
    hwSpotpearHtml: '💸 <strong>Spotpear 1.28" BOX (חלופה זולה — ניסיוני)</strong><br /><span style="font-size:0.76rem;font-weight:400;color:var(--text-muted);">כולל מסך עגול, מגע, רמקול, מיקרופון, סוללה ומארז (~₪77). <strong>טרם נוסה פיזית!</strong></span>',
    hwSpotpearTipHtml: '🧪 אם בחרתם ב-Spotpear להורדת עלויות: הקפידו לבחור במוצר את האופציה <strong>With Touch &amp; Battery</strong>, ובשלב 5 באשף בחרו בדגם <strong>Spotpear 1.28" BOX (ניסיוני)</strong>. נשמח למשוב ממי שמזמין!',
    hwStlBtn: "🖨️ מארז מודפס / קבצי STL",
    hwGuideBtn: "📖 מדריך מלא (GitHub)",

    // Dynamic status & alert messages
    stKeyInvalid: "נא להזין מפתח Gemini תקין (מתחיל בדרך כלל ב-AIza).",
    stKeyChecking: "בודק את המפתח מול שרתי Google Gemini...",
    stKeyRejected: (msg) => `המפתח נדחה: ${msg}`,
    stKeyOk: "מעולה! המפתח תקין ופעיל מול Google Gemini.",
    stKeyNetErr: (msg) => `שגיאת רשת בבדיקת המפתח: ${msg}`,
    stVoiceNeedKey: "כדי לשמוע דגימת קול חיה, הזינו קודם את מפתח ה-Gemini בשלב 1.",
    stVoiceLoading: (v) => `יוצר דגימת קול חיה עבור ${v}...`,
    stVoiceNoAudio: "לא התקבל אודיו מהשרת",
    stVoicePlaying: (v) => `מנגן עכשיו את הקול ${v} — הביטו בפנים של רובי משמאל!`,
    stVoiceDone: (v) => `דגימת הקול (${v}) נוגנה בהצלחה!`,
    stVoiceErr: (msg) => `שגיאה בהשמעת דגימת קול: ${msg}`,
    stCfgTooLarge: (n) => `גודל קובץ ההגדרות חורג מהמותר (${n} בתים)`,
    stFwMissingSpotpear: (url, status) =>
      `קובץ הקושחה הניסיוני עבור Spotpear 1.28" (${url}) אינו זמין עדיין בשרת זה (HTTP ${status}). עבור לוח Waveshare 1.75" בחרו בדגם הראשי, או טענו קובץ .bin מקומי מתחת לכפתור.`,
    stFwMissing: (url, status) =>
      `קובץ הקושחה (${url}) לא נמצא בשרת זה (HTTP ${status}). ניתן לבחור קובץ .bin מקומי מתחת לכפתור או להשתמש באתר GitHub Pages הרשמי.`,
    stNoWebSerial: "הדפדפן הנוכחי אינו תומך ב-WebSerial. נא לפתוח את האשף בדפדפן Google Chrome או Microsoft Edge במחשב.",
    stNoKeyConfirm: "לא הזנתם מפתח Gemini בשלב 1. רובי יידלק אך יבקש מכם להגדיר מפתח דרך http://robi.local לפני שיוכל לדבר. להמשיך בצריבה?",
    stDlUpgrade: "מוריד עדכון קושחה של HeyRobi (ללא נגיעה בזיכרונות ובהגדרות)...",
    stDlFull: "מוריד את קובץ המערכת של HeyRobi...",
    stBadBin: "קובץ הקושחה שהתקבל אינו תמונת ESP32 תקינה (חסר magic 0xE9).",
    stPickPort: "בחרו את רובי בחלון ה-USB שנפתח (USB JTAG/serial debug unit)...",
    stConnecting: "מתחבר לבקר ESP32-S3 של רובי...",
    stFlashingCfg: "מעדכן הגדרות בזיכרון של רובי (שנייה אחת)...",
    stFlashingUpg: "משדרג קושחה בלבד ושומר על כל הזיכרונות וההגדרות (כ-15–25 שניות, נא לא לנתק)...",
    stFlashingFull: "צורב את מערכת HeyRobi וההגדרות האישיות שלכם (כ-45–90 שניות, נא לא לנתק את הכבל)...",
    stFlashOkUpg: "השדרוג הושלם בהצלחה! כל הזיכרונות, ה-WiFi וההגדרות שלך נשמרו במלואם 🎉",
    stFlashOkFull: "הצריבה הושלמה בהצלחה! רובי מתעורר עכשיו ויתחבר אוטומטית ל-WiFi הביתי שלכם 🎉",
    stFlashErr: (msg) => `שגיאה בצריבה: ${msg}`,
    stProfileLoaded: "הפרופיל נטען בהצלחה!",
    stProfileInvalid: (msg) => `קובץ ה-JSON אינו תקין: ${msg}`,
    stCustomBinSelected: (name) => `נבחר קובץ קושחה מקומי: ${name}`,

    // Defaults
    defaultRobotName: "רובי",
    defaultLanguage: "he-IL",
    defaultKidName: "נועה",
    defaultKidInterests: "חלל, חיות, חידות וסיפורים",
    newKidInterests: "משחקים, חידות וסיפורים",

    // Voices
    voices: [
      { id: "Puck", label: "Puck ⭐ מומלץ", desc: "קופצני, שמח, אנרגטי וטבעי מאוד לילדים" },
      { id: "Zephyr", label: "Zephyr", desc: "בהיר, נעים, קליל וידידותי" },
      { id: "Leda", label: "Leda", desc: "צעיר, חמים וסקרן" },
      { id: "Aoede", label: "Aoede", desc: "עשיר, מלודי ומספר סיפורים מצוין" },
      { id: "Fenrir", label: "Fenrir", desc: "נלהב, הרפתקני ומלא מרץ" },
      { id: "Achird", label: "Achird", desc: "רגוע, חברי ונעים לאוזן" },
      { id: "Kore", label: "Kore", desc: "ברור, סבלני ומעולה לתרגול לימודי" },
      { id: "Charon", label: "Charon", desc: "יציב, סמכותי וידען" },
    ],

    // Activity presets
    presets: [
      { label: "חשבון", prompt: "בוא נתרגל חשבון בכיף! תשאל אותי חידות ותרגילים מותאמים לגיל שלי ותעודד אותי." },
      { label: "אנגלית", prompt: "בוא נלמד אנגלית במשחק! תלמד אותי מילים ומשפטים חדשים ותבקש ממני לחזור אחריך." },
      { label: "ידע כללי", prompt: "בוא נשחק טריוויה וידע כללי על מדע, טבע, חיות והעולם!" },
      { label: "סיפור בהמשכים", prompt: "בוא נמציא יחד סיפור הרפתקאות מרתק שבהמשך כל קטע אני מחליט מה הגיבור עושה!" },
      { label: "חלל ומדע", prompt: "ספר לי עובדה מדהימה על החלל, כוכבי לכת או המצאות מדעיות ושאל אותי מה דעתי!" },
      { label: "חידות בלשיות", prompt: "תן לי חידת היגיון או תעלומה בלשית קצרה ותן לי רמזים עד שאפתור!" },
      { label: "דינוזאורים וחיות", prompt: "בוא נחקור דינוזאורים וחיות פרא נדירות עם חידון נכון או לא נכון!" },
    ],

    // Face Styles & Themes
    faceStyles: {
      cyber_visor: { title: "קפסולות סייבר", subtitle: "עיני זכוכית אנכיות עם סריקת לייזר" },
      round_kawaii: { title: "עגול חמוד", subtitle: "אישונים גדולים ונוצצים עם לחיים סמוקות" },
      retro_pixel: { title: "רטרו פיקסל", subtitle: "רובוט ארקייד מרובע בסגנון 8-ביט" },
      visor_cyclops: { title: "משקף סייבורג", subtitle: "קסדת משקף רחבה עם אלומת סורק נעה" },
      owl_mech: { title: "עדשות מכניות", subtitle: "טבעות צמצם כפולות בסגנון רובוט מכני" },
    },
    faceThemes: {
      cyber_cyan: { title: "סייבר טורקיז", subtitle: "הניאון הקלאסי הזוהר" },
      solar_gold: { title: "זהב סולארי", subtitle: "חמים, אנרגטי ושמשי" },
      emerald_matrix: { title: "מטריקס אזמרגד", subtitle: "מדע בדיוני ירוק־ניאון חד" },
      cosmic_violet: { title: "סגול קוסמי", subtitle: "חלל עמוק ונצנוץ גלקטי" },
      coral_cute: { title: "קורל מתוק", subtitle: "ורוד־אפרסק חמוד וידידותי" },
    },

    // Simulator canvas text
    simGreeting: (name) => `היי, אני ${name}!`,
    simKidMenuTitle: (name) => `${name} — מה עושים?`,
    simBackToFace: "חזרה לפנים",
    simKidFallback: (i) => `ילד ${i}`,
    simActFallback: (i) => `פעילות ${i}`,
  },
};

export function getDict(lang) {
  return I18N[lang === "he" ? "he" : "en"];
}
