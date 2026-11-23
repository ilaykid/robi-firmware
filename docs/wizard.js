import { FACE_STYLES, FACE_THEMES, RobiFaceSimulator } from "./face-preview.js";
import { ESPLoader, Transport } from "./vendor/esptool-js.bundle.js";
import { I18N, getDict } from "./i18n.js";

const STORAGE_KEY = "heyrobi_wizard_draft_v2";
const UI_LANG_KEY = "heyrobi_wizard_ui_lang_v1";

function detectInitialUiLang() {
  try {
    const params = new URLSearchParams(window.location.search);
    const q = (params.get("lang") || "").toLowerCase();
    if (q === "he" || q === "he-il") return "he";
    if (q === "en" || q === "en-us") return "en";
    const saved = localStorage.getItem(UI_LANG_KEY);
    if (saved === "he" || saved === "en") return saved;
  } catch (_) {}
  return "en";
}

function makeDefaultKids(lang) {
  const d = getDict(lang);
  return [
    {
      name: d.defaultKidName,
      age: 8,
      color: "#3b82f6",
      interests: d.defaultKidInterests,
      activities: d.presets.slice(0, 4).map((p) => ({ ...p })),
    },
  ];
}

function isUntouchedDefaultKids(kids, lang) {
  const def = makeDefaultKids(lang);
  if (!Array.isArray(kids) || kids.length !== 1) return false;
  const k = kids[0];
  const d = def[0];
  if (k.name !== d.name || k.interests !== d.interests) return false;
  if (!Array.isArray(k.activities) || k.activities.length !== 4) return false;
  return k.activities.every((a, i) => a.label === d.activities[i].label && a.prompt === d.activities[i].prompt);
}

let uiLang = detectInitialUiLang();
const initDict = getDict(uiLang);

const state = {
  step: 1,
  gemini_key: "",
  preferred_model: "auto",
  persona_mode: "hybrid",
  keyVerified: false,
  robot_name: initDict.defaultRobotName,
  language: initDict.defaultLanguage,
  voice: "Puck",
  face_style: "cyber_visor",
  face_theme: "cyber_cyan",
  volume: 80,
  brightness: 85,
  custom_topics: [],
  custom_prompt: "",
  kids: makeDefaultKids(uiLang),
  wifi_ssid: "",
  wifi_pass: "",
  board: "waveshare175",
  wake_variant: "heyrobby",
  customBinFile: null,
};

let simulator = null;
let activeAudioCtx = null;
let activeAudioSource = null;
const nextSlotByKid = {};

function dict() {
  return getDict(uiLang);
}

function applyStaticI18n() {
  const d = dict();
  document.documentElement.lang = uiLang;
  document.documentElement.dir = d.dir;
  document.title = d.pageTitle;

  document.querySelectorAll("[data-i18n]").forEach((el) => {
    const key = el.dataset.i18n;
    if (typeof d[key] === "string") el.textContent = d[key];
  });
  document.querySelectorAll("[data-i18n-html]").forEach((el) => {
    const key = el.dataset.i18nHtml;
    if (typeof d[key] === "string") el.innerHTML = d[key];
  });
  document.querySelectorAll("[data-i18n-placeholder]").forEach((el) => {
    const key = el.dataset.i18nPlaceholder;
    if (typeof d[key] === "string") el.placeholder = d[key];
  });
  document.querySelectorAll("[data-i18n-aria]").forEach((el) => {
    const key = el.dataset.i18nAria;
    if (typeof d[key] === "string") el.setAttribute("aria-label", d[key]);
  });

  document.querySelectorAll(".lang-btn[data-ui-lang]").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.uiLang === uiLang);
  });

  const lnkCase = document.getElementById("lnkOrderCaseIL");
  if (lnkCase) {
    lnkCase.href =
      uiLang === "he"
        ? "https://github.com/ilaykid/robi-firmware/blob/main/case/Robi_v4/README.md#hebrew-guide"
        : "https://github.com/ilaykid/robi-firmware/tree/main/case/Robi_v4";
  }
  const lnkGuide = document.getElementById("lnkFullGuide");
  if (lnkGuide) {
    lnkGuide.href =
      uiLang === "he"
        ? "https://github.com/ilaykid/robi-firmware#hebrew-readme"
        : "https://github.com/ilaykid/robi-firmware";
  }
}

function setUiLang(nextLang, { persist = true } = {}) {
  const normalized = nextLang === "he" ? "he" : "en";
  const prevLang = uiLang;
  const prevDict = getDict(prevLang);
  const nextDict = getDict(normalized);

  if (prevLang !== normalized) {
    if (state.robot_name === prevDict.defaultRobotName) {
      state.robot_name = nextDict.defaultRobotName;
    }
    if (state.language === prevDict.defaultLanguage) {
      state.language = nextDict.defaultLanguage;
    }
    if (isUntouchedDefaultKids(state.kids, prevLang)) {
      state.kids = makeDefaultKids(normalized);
    }
  }

  uiLang = normalized;
  if (persist) {
    try {
      localStorage.setItem(UI_LANG_KEY, uiLang);
    } catch (_) {}
  }

  applyStaticI18n();
  hydrateInputs();
  renderVoices();
  renderFaceStyles();
  renderThemes();
  renderCustomTopics();
  renderKids();
  syncSimulator();
}

function loadDraft() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const saved = JSON.parse(raw);
    if (saved.preferred_model) state.preferred_model = saved.preferred_model;
    if (saved.persona_mode === "kids" || saved.persona_mode === "assistant" || saved.persona_mode === "hybrid") {
      state.persona_mode = saved.persona_mode;
    }
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
    if (saved.board === "waveshare175" || saved.board === "spotpear128") state.board = saved.board;
    if (saved.wake_variant) state.wake_variant = saved.wake_variant;
    // Security: scrub any legacy plaintext secrets from localStorage immediately
    if (saved.gemini_key !== undefined || saved.wifi_pass !== undefined || saved.wifi_ssid !== undefined) {
      delete saved.gemini_key;
      delete saved.wifi_pass;
      delete saved.wifi_ssid;
      localStorage.setItem(STORAGE_KEY, JSON.stringify(saved));
    }
  } catch (_) {}
}

function saveDraft() {
  try {
    // Security: never store gemini_key, wifi_ssid, or wifi_pass in persistent browser localStorage
    const safeCopy = {
      preferred_model: state.preferred_model,
      persona_mode: state.persona_mode,
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
      board: state.board,
      wake_variant: state.wake_variant,
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
    kids: state.persona_mode === "assistant" ? [] : state.kids,
    uiLang,
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
  const d = dict();
  const key = state.gemini_key.trim();
  if (!key || key.length < 20) {
    setStatus("keyStatus", d.stKeyInvalid, "err");
    return;
  }
  const btn = document.getElementById("btnVerifyKey");
  btn.disabled = true;
  setStatus("keyStatus", d.stKeyChecking, "warn");
  try {
    const res = await fetch(
      `https://generativelanguage.googleapis.com/v1beta/models?pageSize=5&key=${encodeURIComponent(key)}`
    );
    if (!res.ok) {
      const errJson = await res.json().catch(() => ({}));
      const msg = errJson?.error?.message || `HTTP ${res.status}`;
      setStatus("keyStatus", d.stKeyRejected(msg), "err");
      state.keyVerified = false;
      return;
    }
    state.keyVerified = true;
    setStatus("keyStatus", d.stKeyOk, "ok");
  } catch (e) {
    setStatus("keyStatus", d.stKeyNetErr(e.message), "err");
  } finally {
    btn.disabled = false;
  }
}

// ---------- Step 2: Voice Selection & Browser Live TTS Sample ----------
function renderVoices() {
  const d = dict();
  const grid = document.getElementById("voiceGrid");
  grid.innerHTML = "";
  for (const v of d.voices) {
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
  const d = dict();
  const key = state.gemini_key.trim();
  if (!key) {
    setStatus("voiceStatus", d.stVoiceNeedKey, "warn");
    return;
  }
  const btn = document.getElementById("btnPlayVoice");
  btn.disabled = true;
  setStatus("voiceStatus", d.stVoiceLoading(state.voice), "warn");

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
    if (!b64) throw new Error(d.stVoiceNoAudio);

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
      setStatus("voiceStatus", dict().stVoiceDone(state.voice), "ok");
    };

    setStatus("voiceStatus", d.stVoicePlaying(state.voice), "ok");
    source.start();
    pumpLipSync();
  } catch (e) {
    setStatus("voiceStatus", dict().stVoiceErr(e.message), "err");
  } finally {
    btn.disabled = false;
  }
}

// ---------- Step 3: Face Styles & Color Themes ----------
function renderFaceStyles() {
  const d = dict();
  const grid = document.getElementById("styleGrid");
  grid.innerHTML = "";
  for (const st of Object.values(FACE_STYLES)) {
    const loc = d.faceStyles[st.id] || st;
    const card = document.createElement("div");
    card.className = "style-card" + (state.face_style === st.id ? " selected" : "");
    card.innerHTML = `
      <div class="style-icon">${st.icon}</div>
      <div class="style-info">
        <strong>${loc.title}</strong>
        <span>${loc.subtitle}</span>
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
  const d = dict();
  const grid = document.getElementById("themeGrid");
  grid.innerHTML = "";
  for (const t of Object.values(FACE_THEMES)) {
    const loc = d.faceThemes[t.id] || t;
    const card = document.createElement("div");
    card.className = "theme-card" + (state.face_theme === t.id ? " selected" : "");
    card.innerHTML = `
      <div class="theme-dot" style="background: radial-gradient(circle at 35% 35%, ${t.eyeCore}, ${t.eyeMain} 65%, ${t.bgDeep}); color: ${t.eyeMain};"></div>
      <div class="theme-info">
        <strong>${loc.title}</strong>
        <span>${loc.subtitle}</span>
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
  const d = dict();
  const list = document.getElementById("customTopicsList");
  list.innerHTML = "";
  if (state.custom_topics.length === 0) {
    list.innerHTML = `<span style="color:var(--text-muted);font-size:0.82rem;">${d.s4TopicsEmpty}</span>`;
    return;
  }
  state.custom_topics.forEach((topic, idx) => {
    const tag = document.createElement("span");
    tag.className = "topic-tag";
    tag.innerHTML = `<span>${escapeAttr(topic)}</span><button type="button" title="${escapeAttr(d.s4RemoveTopicTitle)}" data-rm-topic="${idx}">×</button>`;
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
  const d = dict();
  const inp = document.getElementById("inpNewTopic");
  const val = (inp.value || "").trim();
  if (!val) return;
  if (state.custom_topics.length >= 16) {
    alert(d.s4MaxTopicsAlert);
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
  const presets = dict().presets;
  if (!Array.isArray(kid.activities)) kid.activities = [];
  while (kid.activities.length < 4) {
    const fallback = presets[kid.activities.length % presets.length];
    kid.activities.push({ ...fallback });
  }
  kid.activities = kid.activities.slice(0, 4);
}

function renderKids() {
  const d = dict();
  const presets = d.presets;
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
          <input type="text" data-k="${idx}" data-a="${aIdx}" data-f="label" maxlength="16" placeholder="${escapeAttr(d.kidActLabelPh(aIdx + 1))}" value="${escapeAttr(act.label)}" />
          <input type="text" data-k="${idx}" data-a="${aIdx}" data-f="prompt" maxlength="200" placeholder="${escapeAttr(d.kidActPromptPh)}" value="${escapeAttr(act.prompt)}" />
        </div>
      `
      )
      .join("");

    const presetHtml = presets
      .map(
        (p, pIdx) =>
          `<button type="button" class="chip" data-preset="${pIdx}" data-kid="${idx}">+ ${escapeAttr(p.label)}</button>`
      )
      .join("");

    const displayName = escapeAttr(kid.name || d.kidFallbackName(idx + 1));

    box.innerHTML = `
      <div class="kid-head">
        <strong>${d.kidHeader(idx + 1)}</strong>
        <button type="button" class="btn btn-secondary" data-remove-kid="${idx}" style="padding:5px 12px;font-size:0.8rem;">${d.kidRemove}</button>
      </div>
      <div class="row-2">
        <div class="field">
          <label>${d.kidNameLabel}</label>
          <input type="text" data-kid-field="name" data-k="${idx}" maxlength="14" value="${escapeAttr(kid.name)}" />
        </div>
        <div class="row-2">
          <div class="field">
            <label>${d.kidAgeLabel}</label>
            <input type="number" data-kid-field="age" data-k="${idx}" min="3" max="18" value="${Number(kid.age) || 8}" />
          </div>
          <div class="field">
            <label>${d.kidColorLabel}</label>
            <input type="color" data-kid-field="color" data-k="${idx}" value="${escapeAttr(kid.color || "#3b82f6")}" style="height:43px;width:100%;padding:4px;background:var(--bg-input);border-radius:10px;border:1px solid rgba(148,163,184,0.26);cursor:pointer;" />
          </div>
        </div>
      </div>
      <div class="field">
        <label>${d.kidInterestsLabel}</label>
        <input type="text" data-kid-field="interests" data-k="${idx}" maxlength="180" placeholder="${escapeAttr(d.kidInterestsPlaceholder)}" value="${escapeAttr(kid.interests || "")}" />
      </div>
      <div class="field" style="margin-bottom:0;">
        <label>${d.kidActivitiesLabel(displayName)}</label>
        <div class="custom-act-builder">
          <div style="font-size:0.8rem;color:#a5f3fc;font-weight:600;margin-bottom:6px;">${d.kidCustomActTitle}</div>
          <div style="display:grid;grid-template-columns:130px 1fr auto;gap:8px;align-items:center;">
            <input type="text" id="custActLbl_${idx}" maxlength="16" placeholder="${escapeAttr(d.kidCustomActLabelPh)}" />
            <input type="text" id="custActPrm_${idx}" maxlength="200" placeholder="${escapeAttr(d.kidCustomActPromptPh)}" />
            <button type="button" class="btn btn-accent" data-add-cust-act="${idx}" style="padding:9px 13px;font-size:0.82rem;">${d.kidCustomActBtn}</button>
          </div>
        </div>
        <div style="font-size:0.78rem;color:var(--text-muted);margin-bottom:4px;">${d.kidPresetsHint}</div>
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
      const prm = (prmInp?.value || "").trim() || (lbl ? dict().kidDefaultPrompt(lbl) : "");
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
      const p = dict().presets[Number(chip.dataset.preset)];
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
  const d = dict();
  const rName = (state.robot_name || d.defaultRobotName).trim();
  const fTheme = state.face_theme || "cyber_cyan";
  const obj = {
    bot_name: rName,
    robot_name: rName,
    preferred_model: state.preferred_model || "auto",
    persona_mode: state.persona_mode || "hybrid",
    voice: state.voice || "Puck",
    language: state.language || d.defaultLanguage,
    theme: fTheme,
    face_theme: fTheme,
    face_style: state.face_style || "cyber_visor",
    volume: Number(state.volume) || 80,
    brightness: Number(state.brightness) || 85,
    custom_topics: (state.custom_topics || []).slice(0, 16),
    custom_prompt: (state.custom_prompt || "").trim(),
    kids: state.kids.slice(0, 3).map((k, idx) => ({
      name: (k.name || d.simKidFallback(idx + 1)).trim(),
      age: Math.max(3, Math.min(18, Number(k.age) || 8)),
      color: k.color || "#3b82f6",
      interests: (k.interests || "").trim(),
      activities: (k.activities || []).slice(0, 4).map((a) => ({
        label: (a.label || d.kidFallbackAct).trim(),
        prompt: (a.prompt || "").trim(),
      })),
    })),
  };
  if (includeSecrets) {
    const trimmedKey = state.gemini_key.trim();
    obj.gemini_key = trimmedKey.length >= 20 ? trimmedKey : "__KEEP__";
    obj.wifi_ssid = state.wifi_ssid.trim();
    obj.wifi_pass = state.wifi_pass;
  }
  return obj;
}

export function buildRobiCfgSectorBytes(cfgObj) {
  const jsonBytes = new TextEncoder().encode(JSON.stringify(cfgObj));
  if (jsonBytes.length === 0 || jsonBytes.length > 12288) {
    throw new Error(dict().stCfgTooLarge(jsonBytes.length));
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

async function fetchFirmwareBytes({ appOnly = false } = {}) {
  const d = dict();
  if (state.customBinFile) {
    appendLog(`Loading local firmware file: ${state.customBinFile.name}`);
    const ab = await state.customBinFile.arrayBuffer();
    const raw = new Uint8Array(ab);
    if (appOnly && raw.length > 0x500000) {
      appendLog("Extracting app partition (0x20000) from merged binary to preserve NVS memories...");
      return raw.subarray(0x20000, Math.min(raw.length, 0x410000));
    }
    return raw;
  }
  const variant = state.wake_variant === "hiesp" ? "hiesp" : "heyrobby";
  const prefix = appOnly ? "robi-app" : "robi-full";
  const boardTag = state.board === "spotpear128" ? "-spotpear128" : "";
  const url = `./firmware/${prefix}${boardTag}-${variant}.bin`;
  appendLog(`Downloading pre-built firmware: ${url} ...`);
  const res = await fetch(`${url}?t=${Date.now()}`, { cache: "no-store" });
  if (!res.ok) {
    if (state.board === "spotpear128") {
      throw new Error(d.stFwMissingSpotpear(url, res.status));
    }
    throw new Error(d.stFwMissing(url, res.status));
  }
  const ab = await res.arrayBuffer();
  return new Uint8Array(ab);
}

async function runUsbFlash({ configOnly = false, upgradeOnly = false }) {
  const d = dict();
  if (!("serial" in navigator)) {
    setStatus("flashStatus", d.stNoWebSerial, "err");
    return;
  }

  if (!upgradeOnly && !state.gemini_key.trim()) {
    const ok = window.confirm(d.stNoKeyConfirm);
    if (!ok) return;
  }

  const btnFull = document.getElementById("btnFlashFull");
  const btnUpg = document.getElementById("btnFlashUpgradeOnly");
  const btnCfg = document.getElementById("btnFlashConfigOnly");
  btnFull.disabled = true;
  if (btnUpg) btnUpg.disabled = true;
  btnCfg.disabled = true;
  document.getElementById("flashConsole").textContent = "";

  let transport = null;
  try {
    const fileArray = [];
    if (upgradeOnly) {
      setStatus("flashStatus", d.stDlUpgrade, "warn");
      setProgress(4);
      const appBytes = await fetchFirmwareBytes({ appOnly: true });
      if (appBytes.length < 200000 || appBytes[0] !== 0xe9) {
        throw new Error(d.stBadBin);
      }
      appendLog(`Upgrade app loaded (${(appBytes.length / (1024 * 1024)).toFixed(2)} MB -> 0x20000, NVS 0x9000 untouched)`);
      fileArray.push({ data: appBytes, address: 0x20000 });
    } else {
      const cfgObj = buildConfigObject({ includeSecrets: true });
      const cfgSector = buildRobiCfgSectorBytes(cfgObj);
      if (!configOnly) {
        setStatus("flashStatus", d.stDlFull, "warn");
        setProgress(4);
        const fwBytes = await fetchFirmwareBytes({ appOnly: false });
        if (fwBytes.length < 200000 || fwBytes[0] !== 0xe9) {
          throw new Error(d.stBadBin);
        }
        appendLog(`Firmware loaded (${(fwBytes.length / (1024 * 1024)).toFixed(2)} MB)`);
        fileArray.push({ data: fwBytes, address: 0x0 });
      }
      fileArray.push({ data: cfgSector, address: 0x010000 });
    }

    setStatus("flashStatus", d.stPickPort, "warn");
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

    setStatus("flashStatus", d.stConnecting, "warn");
    const chip = await loader.main();
    appendLog(`Connected to ${chip}`);

    const totalBytes = fileArray.reduce((acc, f) => acc + f.data.length, 0);
    let completedPrior = 0;

    setStatus(
      "flashStatus",
      configOnly ? d.stFlashingCfg : upgradeOnly ? d.stFlashingUpg : d.stFlashingFull,
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
      upgradeOnly ? d.stFlashOkUpg : d.stFlashOkFull,
      "ok"
    );
  } catch (e) {
    appendLog(`ERROR: ${e?.message || e}`);
    setStatus("flashStatus", dict().stFlashErr(e?.message || e), "err");
  } finally {
    if (transport) {
      try { await transport.disconnect(); } catch (_) {}
    }
    btnFull.disabled = false;
    if (btnUpg) btnUpg.disabled = false;
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
      if (data.preferred_model) state.preferred_model = data.preferred_model;
      if (data.persona_mode === "kids" || data.persona_mode === "assistant" || data.persona_mode === "hybrid") {
        state.persona_mode = data.persona_mode;
      }
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
      alert(dict().stProfileLoaded);
    } catch (e) {
      alert(dict().stProfileInvalid(e.message));
    }
  };
  reader.readAsText(file);
}

function syncKidsSectionVisibility() {
  const wrap = document.getElementById("kidsSectionWrap");
  if (wrap) {
    wrap.style.display = state.persona_mode === "assistant" ? "none" : "block";
  }
}

function hydrateInputs() {
  document.getElementById("inpGeminiKey").value = state.gemini_key;
  const selMod = document.getElementById("selPreferredModel");
  if (selMod) selMod.value = state.preferred_model || "auto";
  const selPm = document.getElementById("selPersonaMode");
  if (selPm) selPm.value = state.persona_mode || "hybrid";
  syncKidsSectionVisibility();
  document.getElementById("inpRobotName").value = state.robot_name;
  document.getElementById("selLanguage").value = state.language;
  document.getElementById("rngVolume").value = state.volume;
  document.getElementById("lblVolume").textContent = `${state.volume}%`;
  document.getElementById("rngBrightness").value = state.brightness;
  document.getElementById("lblBrightness").textContent = `${state.brightness}%`;
  document.getElementById("txtCustomPrompt").value = state.custom_prompt;
  document.getElementById("inpWifiSsid").value = state.wifi_ssid;
  document.getElementById("inpWifiPass").value = state.wifi_pass;
  const selBoard = document.getElementById("selBoard");
  if (selBoard) selBoard.value = state.board || "waveshare175";
  const expNote = document.getElementById("boardExperimentalNote");
  if (expNote) expNote.style.display = state.board === "spotpear128" ? "block" : "none";
  document.getElementById("selWakeVariant").value = state.wake_variant;
}

// ---------- Boot ----------
window.addEventListener("DOMContentLoaded", () => {
  loadDraft();

  const canvas = document.getElementById("robiCanvas");
  simulator = new RobiFaceSimulator(canvas);

  applyStaticI18n();
  syncSimulator();

  hydrateInputs();
  renderVoices();
  renderFaceStyles();
  renderThemes();
  renderCustomTopics();
  renderKids();

  document.querySelectorAll(".lang-btn[data-ui-lang]").forEach((btn) => {
    btn.addEventListener("click", () => setUiLang(btn.dataset.uiLang));
  });

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
  const selMod = document.getElementById("selPreferredModel");
  if (selMod) {
    selMod.addEventListener("change", (e) => {
      state.preferred_model = e.target.value;
      saveDraft();
    });
  }
  const selPm = document.getElementById("selPersonaMode");
  if (selPm) {
    selPm.addEventListener("change", (e) => {
      state.persona_mode = e.target.value;
      syncKidsSectionVisibility();
      saveDraft();
    });
  }
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
    const d = dict();
    const palette = ["#3b82f6", "#10b981", "#f59e0b"];
    state.kids.push({
      name: d.simKidFallback(state.kids.length + 1),
      age: 7,
      color: palette[state.kids.length % palette.length],
      interests: d.newKidInterests,
      activities: d.presets.slice(0, 4).map((p) => ({ ...p })),
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
  const selBoard = document.getElementById("selBoard");
  if (selBoard) {
    selBoard.addEventListener("change", (e) => {
      state.board = e.target.value;
      const expNote = document.getElementById("boardExperimentalNote");
      if (expNote) expNote.style.display = state.board === "spotpear128" ? "block" : "none";
      saveDraft();
    });
  }
  document.getElementById("selWakeVariant").addEventListener("change", (e) => {
    state.wake_variant = e.target.value;
    saveDraft();
  });

  document.getElementById("inpCustomBin").addEventListener("change", (e) => {
    state.customBinFile = e.target.files?.[0] || null;
    if (state.customBinFile) {
      setStatus("flashStatus", dict().stCustomBinSelected(state.customBinFile.name), "ok");
    }
  });

  document.getElementById("btnFlashFull").addEventListener("click", () =>
    runUsbFlash({ configOnly: false, upgradeOnly: false })
  );
  const btnFlashUpg = document.getElementById("btnFlashUpgradeOnly");
  if (btnFlashUpg) {
    btnFlashUpg.addEventListener("click", () =>
      runUsbFlash({ configOnly: false, upgradeOnly: true })
    );
  }
  document.getElementById("btnFlashConfigOnly").addEventListener("click", () =>
    runUsbFlash({ configOnly: true, upgradeOnly: false })
  );

  document.getElementById("btnExportJson").addEventListener("click", exportJsonFile);
  document.getElementById("inpImportJson").addEventListener("change", (e) => {
    const f = e.target.files?.[0];
    if (f) importJsonFile(f);
    e.target.value = "";
  });
});
