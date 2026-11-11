import { FACE_STYLES, FACE_THEMES, RobiFaceSimulator } from "./face-preview.js";
import { ESPLoader, Transport } from "./vendor/esptool-js.bundle.js";

const VOICES = [
  { id: "Puck", label: "Puck ⭐ מומלץ", desc: "קופצני, שמח, אנרגטי וטבעי מאוד לילדים" },
  { id: "Zephyr", label: "Zephyr", desc: "בהיר, נעים, קליל וידידותי" },
  { id: "Leda", label: "Leda", desc: "צעיר, חמים וסקרן" },
  { id: "Aoede", label: "Aoede", desc: "עשיר, מלודי ומספר סיפורים מצוין" },
  { id: "Fenrir", label: "Fenrir", desc: "נלהב, הרפתקני ומלא מרץ" },
  { id: "Achird", label: "Achird", desc: "רגוע, חברי ונעים לאוזן" },
  { id: "Kore", label: "Kore", desc: "ברור, סבלני ומעולה לתרגול לימודי" },
  { id: "Charon", label: "Charon", desc: "יציב, סמכותי וידען" },
];

const ACTIVITY_PRESETS = [
  {
    label: "חשבון",
    prompt: "בוא נתרגל חשבון בכיף! תשאל אותי חידות ותרגילים מותאמים לגיל שלי ותעודד אותי.",
  },
  {
    label: "אנגלית",
    prompt: "בוא נלמד אנגלית במשחק! תלמד אותי מילים ומשפטים חדשים ותבקש ממני לחזור אחריך.",
  },
  {
    label: "ידע כללי",
    prompt: "בוא נשחק טריוויה וידע כללי על מדע, טבע, חיות והעולם!",
  },
  {
    label: "סיפור בהמשכים",
    prompt: "בוא נמציא יחד סיפור הרפתקאות מרתק שבהמשך כל קטע אני מחליט מה הגיבור עושה!",
  },
  {
    label: "חלל ומדע",
    prompt: "ספר לי עובדה מדהימה על החלל, כוכבי לכת או המצאות מדעיות ושאל אותי מה דעתי!",
  },
  {
    label: "חידות בלשיות",
    prompt: "תן לי חידת היגיון או תעלומה בלשית קצרה ותן לי רמזים עד שאפתור!",
  },
  {
    label: "דינוזאורים וחיות",
    prompt: "בוא נחקור דינוזאורים וחיות פרא נדירות עם חידון נכון או לא נכון!",
  },
];

const DEFAULT_KIDS = [
  {
    name: "נועה",
    age: 8,
    color: "#3b82f6",
    interests: "חלל, חיות, חידות וסיפורים",
    activities: [
      { ...ACTIVITY_PRESETS[0] },
      { ...ACTIVITY_PRESETS[1] },
      { ...ACTIVITY_PRESETS[2] },
      { ...ACTIVITY_PRESETS[3] },
    ],
  },
];

const STORAGE_KEY = "heyrobi_wizard_draft_v2";

const state = {
  step: 1,
  gemini_key: "",
  keyVerified: false,
  robot_name: "רובי",
  language: "he-IL",
  voice: "Puck",
  face_style: "cyber_visor",
  face_theme: "cyber_cyan",
  volume: 80,
  brightness: 85,
  custom_topics: [],
  custom_prompt: "",
  kids: structuredClone(DEFAULT_KIDS),
  wifi_ssid: "",
  wifi_pass: "",
  wake_variant: "heyrobby",
  customBinFile: null,
};

let simulator = null;
let activeAudioCtx = null;
let activeAudioSource = null;
const nextSlotByKid = {};

function loadDraft() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const saved = JSON.parse(raw);
    if (saved.robot_name) state.robot_name = saved.robot_name;
    if (saved.language) state.language = saved.language;
    if (saved.voice) state.voice = saved.voice;
    if (saved.face_style && FACE_STYLES[saved.face_style]) state.face_style = saved.face_style;
    if (saved.face_theme && FACE_THEMES[saved.face_theme]) state.face_theme = saved.face_theme;
    if (typeof saved.volume === "number") state.volume = saved.volume;
    if (typeof saved.brightness === "number") state.brightness = saved.brightness;
    if (Array.isArray(saved.custom_topics)) state.custom_topics = saved.custom_topics.slice(0, 16);
    if (typeof saved.custom_prompt === "string") state.custom_prompt = saved.custom_prompt;
    if (Array.isArray(saved.kids)) state.kids = saved.kids.slice(0, 3);
    if (saved.wake_variant) state.wake_variant = saved.wake_variant;
    if (saved.wifi_ssid) state.wifi_ssid = saved.wifi_ssid;
    if (saved.wifi_pass) state.wifi_pass = saved.wifi_pass;
    if (saved.gemini_key) state.gemini_key = saved.gemini_key;
  } catch (_) {}
}

function saveDraft() {
  try {
    const safeCopy = {
      gemini_key: state.gemini_key,
      robot_name: state.robot_name,
      language: state.language,
      voice: state.voice,
      face_style: state.face_style,
      face_theme: state.face_theme,
      volume: state.volume,
      brightness: state.brightness,
      custom_topics: state.custom_topics,
      custom_prompt: state.custom_prompt,
      kids: state.kids,
      wake_variant: state.wake_variant,
      wifi_ssid: state.wifi_ssid,
      wifi_pass: state.wifi_pass,
    };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(safeCopy));
  } catch (_) {}
  syncSimulator();
}

function syncSimulator() {
  if (!simulator) return;
  simulator.setState({
    styleId: state.face_style,
    themeId: state.face_theme,
    robotName: state.robot_name,
    kids: state.kids,
  });
}

function setStep(n) {
  state.step = Math.max(1, Math.min(5, n));
  document.querySelectorAll(".step-tab").forEach((el) => {
    el.classList.toggle("active", Number(el.dataset.step) === state.step);
  });
  document.querySelectorAll(".step-pane").forEach((el) => {
    el.classList.toggle("active", Number(el.dataset.pane) === state.step);
  });
}

function setStatus(id, text, kind = "") {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.className = "status-msg" + (kind ? ` ${kind}` : "");
}

// ---------- Step 1: Verify Gemini API Key ----------
async function verifyGeminiKey() {
  const key = state.gemini_key.trim();
  if (!key || key.length < 20) {
    setStatus("keyStatus", "נא להזין מפתח Gemini תקין (מתחיל בדרך כלל ב-AIza).", "err");
    return;
  }
  const btn = document.getElementById("btnVerifyKey");
  btn.disabled = true;
  setStatus("keyStatus", "בודק את המפתח מול שרתי Google Gemini...", "warn");
  try {
    const res = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models?pageSize=5&key=${encodeURIComponent(key)}`
    );
    if (!res.ok) {
      const errJson = await res.json().catch(() => ({}));
      const msg = errJson?.error?.message || `שגיאת HTTP ${res.status}`;
      setStatus("keyStatus", `המפתח נדחה: ${msg}`, "err");
      state.keyVerified = false;
      return;
    }
    state.keyVerified = true;
    setStatus("keyStatus", "מעולה! המפתח תקין ופעיל מול Google Gemini.", "ok");
  } catch (e) {
    setStatus("keyStatus", `שגיאת רשת בבדיקת המפתח: ${e.message}`, "err");
  } finally {
    btn.disabled = false;
  }
}

// ---------- Step 2: Voice Selection & Browser Live TTS Sample ----------
function renderVoices() {
  const grid = document.getElementById("voiceGrid");
  grid.innerHTML = "";
  for (const v of VOICES) {
    const card = document.createElement("div");
    card.className = "voice-card" + (state.voice === v.id ? " selected" : "");
    card.innerHTML = `
      <div class="v-name">
        <span>${v.label}</span>
        <span>${state.voice === v.id ? "✓" : ""}</span>
      </div>
      <div class="v-desc">${v.desc}</div>
    `;
    card.addEventListener("click", () => {
      state.voice = v.id;
      saveDraft();
      renderVoices();
    });
    grid.appendChild(card);
  }
}

function pcm24kBase64ToWavBuffer(b64) {
  const bin = atob(b64);
  const pcmLen = bin.length;
  const buf = new ArrayBuffer(44 + pcmLen);
  const view = new DataView(buf);
  const writeStr = (off, s) => {
    for (let i = 0; i < s.length; i++) view.setUint8(off + i, s.charCodeAt(i));
  };
  const sampleRate = 24000;
  writeStr(0, "RIFF");
  view.setUint32(4, 36 + pcmLen, true);
  writeStr(8, "WAVE");
  writeStr(12, "fmt ");
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, 1, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true);
  view.setUint16(34, 16, true);
  writeStr(36, "data");
  view.setUint32(40, pcmLen, true);
  const out = new Uint8Array(buf, 44);
  for (let i = 0; i < pcmLen; i++) out[i] = bin.charCodeAt(i);
  return buf;
}

async function playVoiceSample() {
  const key = state.gemini_key.trim();
  if (!key) {
    setStatus("voiceStatus", "כדי לשמוע דגימת קול חיה, הזינו קודם את מפתח ה-Gemini בשלב 1.", "warn");
    return;
  }
  const btn = document.getElementById("btnPlayVoice");
  btn.disabled = true;
  setStatus("voiceStatus", `יוצר דגימת קול חיה עבור ${state.voice}...`, "warn");

  try {
    if (activeAudioSource) {
      try { activeAudioSource.stop(); } catch (_) {}
      activeAudioSource = null;
    }

    const sampleText =
      state.language === "en-US"
        ? `Hi there! My name is ${state.robot_name || "Robi"}! I am so excited to play, learn, and explore together!`
        : `היי! קוראים לי ${state.robot_name || "רובי"}! איזה כיף להכיר אתכם, בואו נשחק ונחקור יחד!`;

    const resp = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-preview-tts:generateContent?key=${encodeURIComponent(key)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          contents: [{ parts: [{ text: sampleText }] }],
          generationConfig: {
            responseModalities: ["AUDIO"],
            speechConfig: {
              voiceConfig: {
                prebuiltVoiceConfig: { voiceName: state.voice },
              },
            },
          },
        }),
      }
    );

    if (!resp.ok) {
      const err = await resp.json().catch(() => ({}));
      throw new Error(err?.error?.message || `HTTP ${resp.status}`);
    }

    const data = await resp.json();
    const b64 = data?.candidates?.[0]?.content?.parts?.[0]?.inlineData?.data;
    if (!b64) throw new Error("לא התקבל אודיו מהשרת");

    const wavBuf = pcm24kBase64ToWavBuffer(b64);
    if (!activeAudioCtx) {
      activeAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (activeAudioCtx.state === "suspended") await activeAudioCtx.resume();

    const audioBuffer = await activeAudioCtx.decodeAudioData(wavBuf);
    const source = activeAudioCtx.createBufferSource();
    source.buffer = audioBuffer;

    const analyser = activeAudioCtx.createAnalyser();
    analyser.fftSize = 256;
    source.connect(analyser);
    analyser.connect(activeAudioCtx.destination);
    activeAudioSource = source;

    const timeData = new Uint8Array(analyser.fftSize);
    let playing = true;
    const pumpLipSync = () => {
      if (!playing) {
        simulator?.setExternalAudioLevel(0);
        return;
      }
      analyser.getByteTimeDomainData(timeData);
      let sumSq = 0;
      for (let i = 0; i < timeData.length; i++) {
        const v = (timeData[i] - 128) / 128;
        sumSq += v * v;
      }
      const rms = Math.sqrt(sumSq / timeData.length);
      simulator?.setExternalAudioLevel(Math.min(100, rms * 380));
      requestAnimationFrame(pumpLipSync);
    };

    source.onended = () => {
      playing = false;
      simulator?.setExternalAudioLevel(0);
      setStatus("voiceStatus", `דגימת הקול (${state.voice}) נוגנה בהצלחה!`, "ok");
    };

    setStatus("voiceStatus", `מנגן עכשיו את הקול ${state.voice} — הביטו בפנים של רובי משמאל!`, "ok");
    source.start();
    pumpLipSync();
  } catch (e) {
    setStatus("voiceStatus", `שגיאה בהשמעת דגימת קול: ${e.message}`, "err");
  } finally {
    btn.disabled = false;
  }
}

// ---------- Step 3: Face Styles & Color Themes ----------
function renderFaceStyles() {
  const grid = document.getElementById("styleGrid");
  grid.innerHTML = "";
  for (const st of Object.values(FACE_STYLES)) {
    const card = document.createElement("div");
    card.className = "style-card" + (state.face_style === st.id ? " selected" : "");
    card.innerHTML = `
      <div class="style-icon">${st.icon}</div>
      <div class="style-info">
        <strong>${st.title}</strong>
        <span>${st.subtitle}</span>
      </div>
    `;
    card.addEventListener("click", () => {
      state.face_style = st.id;
      saveDraft();
      renderFaceStyles();
    });
    grid.appendChild(card);
  }
}

function renderThemes() {
  const grid = document.getElementById("themeGrid");
  grid.innerHTML = "";
  for (const t of Object.values(FACE_THEMES)) {
    const card = document.createElement("div");
    card.className = "theme-card" + (state.face_theme === t.id ? " selected" : "");
    card.innerHTML = `
      <div class="theme-dot" style="background: radial-gradient(circle at 35% 35%, ${t.eyeCore}, ${t.eyeMain} 65%, ${t.bgDeep}); color: ${t.eyeMain};"></div>
      <div class="theme-info">
        <strong>${t.title}</strong>
        <span>${t.subtitle}</span>
      </div>
    `;
    card.addEventListener("click", () => {
      state.face_theme = t.id;
      saveDraft();
      renderThemes();
    });
    grid.appendChild(card);
  }
}

// ---------- Step 4: Parent Custom Topics + Kids ----------
function renderCustomTopics() {
  const list = document.getElementById("customTopicsList");
  list.innerHTML = "";
  if (state.custom_topics.length === 0) {
    list.innerHTML = `<span style="color:var(--text-muted);font-size:0.82rem;">עדיין לא נוספו נושאים אישיים — הקלידו למעלה כל נושא שתרצו!</span>`;
    return;
  }
  state.custom_topics.forEach((topic, idx) => {
    const tag = document.createElement("span");
    tag.className = "topic-tag";
    tag.innerHTML = `<span>${escapeAttr(topic)}</span><button type="button" title="הסרת נושא" data-rm-topic="${idx}">×</button>`;
    list.appendChild(tag);
  });
  list.querySelectorAll("[data-rm-topic]").forEach((btn) => {
    btn.addEventListener("click", () => {
      state.custom_topics.splice(Number(btn.dataset.rmTopic), 1);
      saveDraft();
      renderCustomTopics();
    });
  });
}

function addCustomTopicFromInput() {
  const inp = document.getElementById("inpNewTopic");
  const val = (inp.value || "").trim();
  if (!val) return;
  if (state.custom_topics.length >= 16) {
    alert("ניתן להוסיף עד 16 נושאי שיחה אישיים.");
    return;
  }
  if (!state.custom_topics.includes(val)) {
    state.custom_topics.push(val);
    saveDraft();
    renderCustomTopics();
  }
  inp.value = "";
  inp.focus();
}

function ensureFourActivities(kid) {
  if (!Array.isArray(kid.activities)) kid.activities = [];
  while (kid.activities.length < 4) {
    const fallback = ACTIVITY_PRESETS[kid.activities.length % ACTIVITY_PRESETS.length];
    kid.activities.push({ ...fallback });
  }
  kid.activities = kid.activities.slice(0, 4);
}

function renderKids() {
  const wrap = document.getElementById("kidsContainer");
  wrap.innerHTML = "";

  state.kids.forEach((kid, idx) => {
    ensureFourActivities(kid);
    const box = document.createElement("div");
    box.className = "kid-box";

    const actRowsHtml = kid.activities
      .map(
        (act, aIdx) => `
        <div class="act-row">
          <input type="text" data-k="${idx}" data-a="${aIdx}" data-f="label" maxlength="16" placeholder="כותרת כפתור #${aIdx + 1}" value="${escapeAttr(act.label)}" />
          <input type="text" data-k="${idx}" data-a="${aIdx}" data-f="prompt" maxlength="200" placeholder="על מה רובי ידבר כשלוחצים על הכפתור?" value="${escapeAttr(act.prompt)}" />
        </div>
      `
      )
      .join("");

    const presetHtml = ACTIVITY_PRESETS.map(
      (p, pIdx) =>
        `<button type="button" class="chip" data-preset="${pIdx}" data-kid="${idx}">+ ${p.label}</button>`
    ).join("");

    box.innerHTML = `
      <div class="kid-head">
        <strong>פרופיל ילד/ה #${idx + 1}</strong>
        <button type="button" class="btn btn-secondary" data-remove-kid="${idx}" style="padding:5px 12px;font-size:0.8rem;">הסרה</button>
      </div>
      <div class="row-2">
        <div class="field">
          <label>שם הילד/ה (מופיע על המסך העגול)</label>
          <input type="text" data-kid-field="name" data-k="${idx}" maxlength="14" value="${escapeAttr(kid.name)}" />
        </div>
        <div class="row-2">
          <div class="field">
            <label>גיל</label>
            <input type="number" data-kid-field="age" data-k="${idx}" min="3" max="18" value="${Number(kid.age) || 8}" />
          </div>
          <div class="field">
            <label>צבע כפתור</label>
            <input type="color" data-kid-field="color" data-k="${idx}" value="${escapeAttr(kid.color || "#3b82f6")}" style="height:43px;width:100%;padding:4px;background:var(--bg-input);border-radius:10px;border:1px solid rgba(148,163,184,0.26);cursor:pointer;" />
          </div>
        </div>
      </div>
      <div class="field">
        <label>תחומי עניין ותחביבים (כתיבה חופשית — רובי ישלב אותם בשיחות!)</label>
        <input type="text" data-kid-field="interests" data-k="${idx}" maxlength="180" placeholder="למשל: לגו, כדורגל, חלל, ציור, שחמט, מיינקראפט..." value="${escapeAttr(kid.interests || "")}" />
      </div>
      <div class="field" style="margin-bottom:0;">
        <label>4 כפתורי נושאים על המסך של ${escapeAttr(kid.name || `ילד/ה #${idx + 1}`)} (כתבו כפתור אישי משלכם או בחרו מתבנית):</label>
        <div class="custom-act-builder">
          <div style="font-size:0.8rem;color:#a5f3fc;font-weight:600;margin-bottom:6px;">✨ הוספת כפתור נושא אישי משלכם למסך:</div>
          <div style="display:grid;grid-template-columns:130px 1fr auto;gap:8px;align-items:center;">
            <input type="text" id="custActLbl_${idx}" maxlength="16" placeholder="שם על הכפתור" />
            <input type="text" id="custActPrm_${idx}" maxlength="200" placeholder="מה תרצו שרובי יעשה? (למשל: שאל אותי חידון על כדורגל עולמי!)" />
            <button type="button" class="btn btn-accent" data-add-cust-act="${idx}" style="padding:9px 13px;font-size:0.82rem;">+ הכנס לכפתור</button>
          </div>
        </div>
        <div style="font-size:0.78rem;color:var(--text-muted);margin-bottom:4px;">או מילוי מהיר מתבנית מוכנה (ואפשר לערוך חופשי כל שורה למטה):</div>
        <div class="preset-chips">${presetHtml}</div>
        ${actRowsHtml}
      </div>
    `;
    wrap.appendChild(box);
  });

  const btnAdd = document.getElementById("btnAddKid");
  btnAdd.disabled = state.kids.length >= 3;

  wrap.querySelectorAll("[data-remove-kid]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const i = Number(btn.dataset.removeKid);
      state.kids.splice(i, 1);
      saveDraft();
      renderKids();
    });
  });

  wrap.querySelectorAll("[data-kid-field]").forEach((inp) => {
    inp.addEventListener("input", () => {
      const k = Number(inp.dataset.k);
      const f = inp.dataset.kidField;
      if (!state.kids[k]) return;
      state.kids[k][f] = f === "age" ? Number(inp.value) || 8 : inp.value;
      saveDraft();
    });
  });

  wrap.querySelectorAll("[data-a]").forEach((inp) => {
    inp.addEventListener("input", () => {
      const k = Number(inp.dataset.k);
      const a = Number(inp.dataset.a);
      const f = inp.dataset.f;
      if (!state.kids[k]?.activities?.[a]) return;
      state.kids[k].activities[a][f] = inp.value;
      saveDraft();
    });
  });

  wrap.querySelectorAll("[data-add-cust-act]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const k = Number(btn.dataset.addCustAct);
      const lblInp = document.getElementById(`custActLbl_${k}`);
      const prmInp = document.getElementById(`custActPrm_${k}`);
      const lbl = (lblInp?.value || "").trim();
      const prm = (prmInp?.value || "").trim() || (lbl ? `בוא נדבר ונשחק בנושא ${lbl}!` : "");
      if (!lbl || !state.kids[k]) return;
      const slot = (nextSlotByKid[k] || 0) % 4;
      nextSlotByKid[k] = slot + 1;
      state.kids[k].activities[slot] = { label: lbl, prompt: prm };
      saveDraft();
      renderKids();
    });
  });

  wrap.querySelectorAll("[data-preset]").forEach((chip) => {
    chip.addEventListener("click", () => {
      const k = Number(chip.dataset.kid);
      const p = ACTIVITY_PRESETS[Number(chip.dataset.preset)];
      if (!state.kids[k] || !p) return;
      const slot = (nextSlotByKid[k] || 0) % 4;
      nextSlotByKid[k] = slot + 1;
      state.kids[k].activities[slot] = { ...p };
      saveDraft();
      renderKids();
    });
  });
}

function escapeAttr(s) {
  return String(s ?? "")
    .replace(/&/g, "&amp;")
    .replace(/"/g, "&quot;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

// ---------- Step 5: Build ROBICFG1 Binary Sector & WebSerial Flash ----------
export function buildConfigObject({ includeSecrets = true } = {}) {
  const rName = (state.robot_name || "רובי").trim();
  const fTheme = state.face_theme || "cyber_cyan";
  const obj = {
    bot_name: rName,
    robot_name: rName,
    voice: state.voice || "Puck",
    language: state.language || "he-IL",
    theme: fTheme,
    face_theme: fTheme,
    face_style: state.face_style || "cyber_visor",
    volume: Number(state.volume) || 80,
    brightness: Number(state.brightness) || 85,
    custom_topics: (state.custom_topics || []).slice(0, 16),
    custom_prompt: (state.custom_prompt || "").trim(),
    kids: state.kids.slice(0, 3).map((k) => ({
      name: (k.name || "ילד").trim(),
      age: Math.max(3, Math.min(18, Number(k.age) || 8)),
      color: k.color || "#3b82f6",
      interests: (k.interests || "").trim(),
      activities: (k.activities || []).slice(0, 4).map((a) => ({
        label: (a.label || "פעילות").trim(),
        prompt: (a.prompt || "").trim(),
      })),
    })),
  };
  if (includeSecrets) {
    obj.gemini_key = state.gemini_key.trim();
    obj.wifi_ssid = state.wifi_ssid.trim();
    obj.wifi_pass = state.wifi_pass;
  }
  return obj;
}

export function buildRobiCfgSectorBytes(cfgObj) {
  const jsonBytes = new TextEncoder().encode(JSON.stringify(cfgObj));
  if (jsonBytes.length === 0 || jsonBytes.length > 12288) {
    throw new Error(`גודל קובץ ההגדרות חורג מהמותר (${jsonBytes.length} בתים)`);
  }
  const totalLen = Math.ceil((16 + jsonBytes.length) / 4096) * 4096;
  const sector = new Uint8Array(totalLen);
  sector.fill(0xff);

  const magic = "ROBICFG1";
  for (let i = 0; i < 8; i++) sector[i] = magic.charCodeAt(i);

  const dv = new DataView(sector.buffer);
  dv.setUint32(8, jsonBytes.length, true);
  const seq = (Date.now() >>> 0) || 1;
  dv.setUint32(12, seq, true);

  sector.set(jsonBytes, 16);
  return sector;
}

function appendLog(line) {
  const box = document.getElementById("flashConsole");
  box.style.display = "block";
  box.textContent += line + "\n";
  box.scrollTop = box.scrollHeight;
}

function setProgress(pct) {
  const wrap = document.getElementById("progressWrap");
  const bar = document.getElementById("progressBar");
  wrap.style.display = "block";
  bar.style.width = `${Math.max(0, Math.min(100, pct))}%`;
}

async function fetchFirmwareBytes() {
  if (state.customBinFile) {
    appendLog(`Loading local firmware file: ${state.customBinFile.name}`);
    const ab = await state.customBinFile.arrayBuffer();
    return new Uint8Array(ab);
  }
  const variant = state.wake_variant === "hiesp" ? "hiesp" : "heyrobby";
  const url = `./firmware/robi-full-${variant}.bin`;
  appendLog(`Downloading pre-built firmware: ${url} ...`);
  const res = await fetch(`${url}?t=${Date.now()}`, { cache: "no-store" });
  if (!res.ok) {
    throw new Error(
      `קובץ הקושחה (${url}) לא נמצא בשרת זה (HTTP ${res.status}). ניתן לבחור קובץ .bin מקומי מתחת לכפתור או להשתמש באתר GitHub Pages הרשמי.`
    );
  }
  const ab = await res.arrayBuffer();
  return new Uint8Array(ab);
}

async function runUsbFlash({ configOnly = false }) {
  if (!("serial" in navigator)) {
    setStatus(
      "flashStatus",
      "הדפדפן הנוכחי אינו תומך ב-WebSerial. נא לפתוח את האשף בדפדפן Google Chrome או Microsoft Edge במחשב.",
      "err"
    );
    return;
  }

  if (!state.gemini_key.trim()) {
    const ok = window.confirm(
      "לא הזנתם מפתח Gemini בשלב 1. רובי יידלק אך יבקש מכם להגדיר מפתח דרך http://robi.local לפני שיוכל לדבר. להמשיך בצריבה?"
    );
    if (!ok) return;
  }

  const btnFull = document.getElementById("btnFlashFull");
  const btnCfg = document.getElementById("btnFlashConfigOnly");
  btnFull.disabled = true;
  btnCfg.disabled = true;
  document.getElementById("flashConsole").textContent = "";

  let transport = null;
  try {
    const cfgObj = buildConfigObject({ includeSecrets: true });
    const cfgSector = buildRobiCfgSectorBytes(cfgObj);

    const fileArray = [];
    if (!configOnly) {
      setStatus("flashStatus", "מוריד את קובץ המערכת של HeyRobi...", "warn");
      setProgress(4);
      const fwBytes = await fetchFirmwareBytes();
      if (fwBytes.length < 200000 || fwBytes[0] !== 0xe9) {
        throw new Error("קובץ הקושחה שהתקבל אינו תמונת ESP32 תקינה (חסר magic 0xE9).");
      }
      appendLog(`Firmware loaded (${(fwBytes.length / (1024 * 1024)).toFixed(2)} MB)`);
      fileArray.push({ data: fwBytes, address: 0x0 });
    }
    fileArray.push({ data: cfgSector, address: 0x010000 });

    setStatus("flashStatus", "בחרו את רובי בחלון ה-USB שנפתח (USB JTAG/serial debug unit)...", "warn");
    const port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }],
    }).catch(async () => {
      return await navigator.serial.requestPort();
    });

    transport = new Transport(port, true);
    const loader = new ESPLoader({
      transport,
      baudrate: 921600,
      terminal: {
        clean() {},
        writeLine(data) { appendLog(data); },
        write(data) {
          const box = document.getElementById("flashConsole");
          box.style.display = "block";
          box.textContent += data;
          box.scrollTop = box.scrollHeight;
        },
      },
    });

    setStatus("flashStatus", "מתחבר לבקר ESP32-S3 של רובי...", "warn");
    const chip = await loader.main();
    appendLog(`Connected to ${chip}`);

    const totalBytes = fileArray.reduce((acc, f) => acc + f.data.length, 0);
    let completedPrior = 0;

    setStatus(
      "flashStatus",
      configOnly
        ? "מעדכן הגדרות בזיכרון של רובי (שנייה אחת)..."
        : "צורב את מערכת HeyRobi וההגדרות האישיות שלכם (כ-45–90 שניות, נא לא לנתק את הכבל)...",
      "warn"
    );

    await loader.writeFlash({
      fileArray,
      flashSize: "keep",
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written) => {
        if (written === 0 && fileIndex > 0) {
          completedPrior += fileArray[fileIndex - 1].data.length;
        }
        const overall = ((completedPrior + written) / totalBytes) * 100;
        setProgress(overall);
      },
    });

    setProgress(100);
    appendLog("Flash complete! Resetting HeyRobi...");
    await loader.after("hard_reset");
    await transport.disconnect();
    transport = null;

    setStatus(
      "flashStatus",
      "הצריבה הושלמה בהצלחה! רובי מתעורר עכשיו ויתחבר אוטומטית ל-WiFi הביתי שלכם 🎉",
      "ok"
    );
  } catch (e) {
    appendLog(`ERROR: ${e?.message || e}`);
    setStatus("flashStatus", `שגיאה בצריבה: ${e?.message || e}`, "err");
  } finally {
    if (transport) {
      try { await transport.disconnect(); } catch (_) {}
    }
    btnFull.disabled = false;
    btnCfg.disabled = false;
  }
}

// ---------- Export / Import JSON ----------
function exportJsonFile() {
  const cfg = buildConfigObject({ includeSecrets: false });
  const blob = new Blob([JSON.stringify(cfg, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "heyrobi-profile.json";
  a.click();
  URL.revokeObjectURL(a.href);
}

function importJsonFile(file) {
  const reader = new FileReader();
  reader.onload = () => {
    try {
      const data = JSON.parse(String(reader.result));
      if (data.robot_name || data.bot_name) state.robot_name = data.robot_name || data.bot_name;
      if (data.language) state.language = data.language;
      if (data.voice) state.voice = data.voice;
      if (data.face_style && FACE_STYLES[data.face_style]) state.face_style = data.face_style;
      const th = data.face_theme || data.theme;
      if (th && FACE_THEMES[th]) state.face_theme = th;
      if (typeof data.volume === "number") state.volume = data.volume;
      if (typeof data.brightness === "number") state.brightness = data.brightness;
      if (Array.isArray(data.custom_topics)) state.custom_topics = data.custom_topics.slice(0, 16);
      if (typeof data.custom_prompt === "string") state.custom_prompt = data.custom_prompt;
      if (Array.isArray(data.kids)) state.kids = data.kids.slice(0, 3);
      saveDraft();
      hydrateInputs();
      renderVoices();
      renderFaceStyles();
      renderThemes();
      renderCustomTopics();
      renderKids();
      alert("הפרופיל נטען בהצלחה!");
    } catch (e) {
      alert("קובץ ה-JSON אינו תקין: " + e.message);
    }
  };
  reader.readAsText(file);
}

function hydrateInputs() {
  document.getElementById("inpGeminiKey").value = state.gemini_key;
  document.getElementById("inpRobotName").value = state.robot_name;
  document.getElementById("selLanguage").value = state.language;
  document.getElementById("rngVolume").value = state.volume;
  document.getElementById("lblVolume").textContent = `${state.volume}%`;
  document.getElementById("rngBrightness").value = state.brightness;
  document.getElementById("lblBrightness").textContent = `${state.brightness}%`;
  document.getElementById("txtCustomPrompt").value = state.custom_prompt;
  document.getElementById("inpWifiSsid").value = state.wifi_ssid;
  document.getElementById("inpWifiPass").value = state.wifi_pass;
  document.getElementById("selWakeVariant").value = state.wake_variant;
}

// ---------- Boot ----------
window.addEventListener("DOMContentLoaded", () => {
  loadDraft();

  const canvas = document.getElementById("robiCanvas");
  simulator = new RobiFaceSimulator(canvas);
  syncSimulator();

  hydrateInputs();
  renderVoices();
  renderFaceStyles();
  renderThemes();
  renderCustomTopics();
  renderKids();

  document.querySelectorAll(".step-tab").forEach((tab) => {
    tab.addEventListener("click", () => setStep(Number(tab.dataset.step)));
  });
  document.querySelectorAll("[data-goto]").forEach((btn) => {
    btn.addEventListener("click", () => setStep(Number(btn.dataset.goto)));
  });

  document.querySelectorAll(".emo-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".emo-btn").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      simulator.setEmotion(btn.dataset.emo);
    });
  });

  document.getElementById("inpGeminiKey").addEventListener("input", (e) => {
    state.gemini_key = e.target.value;
    state.keyVerified = false;
    saveDraft();
  });
  document.getElementById("btnVerifyKey").addEventListener("click", verifyGeminiKey);

  document.getElementById("inpRobotName").addEventListener("input", (e) => {
    state.robot_name = e.target.value;
    saveDraft();
  });
  document.getElementById("selLanguage").addEventListener("change", (e) => {
    state.language = e.target.value;
    saveDraft();
  });
  document.getElementById("btnPlayVoice").addEventListener("click", playVoiceSample);

  document.getElementById("rngVolume").addEventListener("input", (e) => {
    state.volume = Number(e.target.value);
    document.getElementById("lblVolume").textContent = `${state.volume}%`;
    saveDraft();
  });
  document.getElementById("rngBrightness").addEventListener("input", (e) => {
    state.brightness = Number(e.target.value);
    document.getElementById("lblBrightness").textContent = `${state.brightness}%`;
    saveDraft();
  });

  document.getElementById("btnAddTopic").addEventListener("click", addCustomTopicFromInput);
  document.getElementById("inpNewTopic").addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      addCustomTopicFromInput();
    }
  });

  document.getElementById("btnAddKid").addEventListener("click", () => {
    if (state.kids.length >= 3) return;
    const palette = ["#3b82f6", "#10b981", "#f59e0b"];
    state.kids.push({
      name: `ילד ${state.kids.length + 1}`,
      age: 7,
      color: palette[state.kids.length % palette.length],
      interests: "משחקים, חידות וסיפורים",
      activities: ACTIVITY_PRESETS.slice(0, 4).map((p) => ({ ...p })),
    });
    saveDraft();
    renderKids();
  });

  document.getElementById("txtCustomPrompt").addEventListener("input", (e) => {
    state.custom_prompt = e.target.value;
    saveDraft();
  });

  document.getElementById("inpWifiSsid").addEventListener("input", (e) => {
    state.wifi_ssid = e.target.value;
    saveDraft();
  });
  document.getElementById("inpWifiPass").addEventListener("input", (e) => {
    state.wifi_pass = e.target.value;
    saveDraft();
  });
  document.getElementById("selWakeVariant").addEventListener("change", (e) => {
    state.wake_variant = e.target.value;
    saveDraft();
  });

  document.getElementById("inpCustomBin").addEventListener("change", (e) => {
    state.customBinFile = e.target.files?.[0] || null;
    if (state.customBinFile) {
      setStatus("flashStatus", `נבחר קובץ קושחה מקומי: ${state.customBinFile.name}`, "ok");
    }
  });

  document.getElementById("btnFlashFull").addEventListener("click", () =>
    runUsbFlash({ configOnly: false })
  );
  document.getElementById("btnFlashConfigOnly").addEventListener("click", () =>
    runUsbFlash({ configOnly: true })
  );

  document.getElementById("btnExportJson").addEventListener("click", exportJsonFile);
  document.getElementById("inpImportJson").addEventListener("change", (e) => {
    const f = e.target.files?.[0];
    if (f) importJsonFile(f);
    e.target.value = "";
  });
});
