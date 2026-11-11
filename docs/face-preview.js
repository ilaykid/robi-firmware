// Live 60fps 466x466 Circular AMOLED Simulator matching firmware scripts/robi_ui.inc
export const FACE_THEMES = {
  cyber_cyan: {
    id: "cyber_cyan",
    title: "סייבר טורקיז",
    subtitle: "העיצוב הקלאסי הזוהר של רובי",
    bgDeep: "#02050a",
    hudRing: "#0c192c",
    hudDotDim: "#0e7490",
    eyeGlow: "#0284c7",
    eyeMain: "#22d3ee",
    eyeCore: "#a5f3fc",
    eyeScan: "#e0f2fe",
    brow: "#38bdf8",
    mouthMain: "#22d3ee",
    mouthIdle: "#06b6d4",
  },
  solar_gold: {
    id: "solar_gold",
    title: "זהב סולארי",
    subtitle: "חמים, אנרגטי ושמשי",
    bgDeep: "#090502",
    hudRing: "#291805",
    hudDotDim: "#b45309",
    eyeGlow: "#d97706",
    eyeMain: "#fbbf24",
    eyeCore: "#fef08a",
    eyeScan: "#fffbeb",
    brow: "#f59e0b",
    mouthMain: "#fbbf24",
    mouthIdle: "#d97706",
  },
  emerald_matrix: {
    id: "emerald_matrix",
    title: "מטריקס אזמרגד",
    subtitle: "מדע בדיוני ירוק-ניאון חד",
    bgDeep: "#020905",
    hudRing: "#062817",
    hudDotDim: "#047857",
    eyeGlow: "#059669",
    eyeMain: "#34d399",
    eyeCore: "#a7f3d0",
    eyeScan: "#ecfdf5",
    brow: "#10b981",
    mouthMain: "#34d399",
    mouthIdle: "#10b981",
  },
  cosmic_violet: {
    id: "cosmic_violet",
    title: "סגול קוסמי",
    subtitle: "חלל עמוק ונצנוץ גלקטי",
    bgDeep: "#06020c",
    hudRing: "#1f0d38",
    hudDotDim: "#6d28d9",
    eyeGlow: "#7c3aed",
    eyeMain: "#a78bfa",
    eyeCore: "#ddd6fe",
    eyeScan: "#f5f3ff",
    brow: "#c084fc",
    mouthMain: "#a78bfa",
    mouthIdle: "#8b5cf6",
  },
  coral_cute: {
    id: "coral_cute",
    title: "קורל מתוק",
    subtitle: "ורוד-אפרסק חמוד וידידותי",
    bgDeep: "#0a0206",
    hudRing: "#2d0b1a",
    hudDotDim: "#be185d",
    eyeGlow: "#e11d48",
    eyeMain: "#fb7185",
    eyeCore: "#fecdd3",
    eyeScan: "#fff1f2",
    brow: "#f472b6",
    mouthMain: "#fb7185",
    mouthIdle: "#f43f5e",
  },
};

function roundRect(ctx, x, y, w, h, r) {
  const rad = Math.max(0, Math.min(r, w / 2, h / 2));
  ctx.beginPath();
  ctx.moveTo(x + rad, y);
  ctx.arcTo(x + w, y, x + w, y + h, rad);
  ctx.arcTo(x + w, y + h, x, y + h, rad);
  ctx.arcTo(x, y + h, x, y, rad);
  ctx.arcTo(x, y, x + w, y, rad);
  ctx.closePath();
}

export class RobiFaceSimulator {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.themeId = "cyber_cyan";
    this.robotName = "רובי";
    this.kids = [];
    this.emotion = "happy";
    this.speakingLevel = 0; // 0..100 external audio level
    this.demoTalking = false;
    this.activeMenuKid = -1; // -1 = closed, 0..2 = open 2x2 menu preview
    this.pillRects = [];
    this.backRect = null;

    this.tick = 0;
    this.blinkPhase = 0;
    this.nextBlinkAt = 110;
    this.lookX = 0;
    this.lookY = 0;
    this.targetLookX = 0;
    this.targetLookY = 0;

    this.canvas.addEventListener("click", (ev) => this.handleClick(ev));
    this.loop = this.loop.bind(this);
    requestAnimationFrame(this.loop);
  }

  setState({ themeId, robotName, kids }) {
    if (themeId && FACE_THEMES[themeId]) this.themeId = themeId;
    if (typeof robotName === "string") this.robotName = robotName || "רובי";
    if (Array.isArray(kids)) {
      this.kids = kids;
      if (this.activeMenuKid >= kids.length) this.activeMenuKid = -1;
    }
  }

  setEmotion(emo) {
    this.emotion = emo;
    this.demoTalking = emo === "speaking";
  }

  setExternalAudioLevel(level0to100) {
    this.speakingLevel = Math.max(0, Math.min(100, level0to100));
  }

  handleClick(ev) {
    const rect = this.canvas.getBoundingClientRect();
    const scaleX = 466 / rect.width;
    const scaleY = 466 / rect.height;
    const x = (ev.clientX - rect.left) * scaleX;
    const y = (ev.clientY - rect.top) * scaleY;

    if (this.activeMenuKid >= 0) {
      if (
        this.backRect &&
        x >= this.backRect.x &&
        x <= this.backRect.x + this.backRect.w &&
        y >= this.backRect.y &&
        y <= this.backRect.y + this.backRect.h
      ) {
        this.activeMenuKid = -1;
      } else {
        this.activeMenuKid = -1;
      }
      return;
    }

    for (const p of this.pillRects) {
      if (x >= p.x && x <= p.x + p.w && y >= p.y && y <= p.y + p.h) {
        this.activeMenuKid = p.idx;
        return;
      }
    }
    // Tap face -> playful wink/blink
    this.blinkPhase = 1;
  }

  loop() {
    this.tick++;
    this.updatePhysics();
    this.render();
    requestAnimationFrame(this.loop);
  }

  updatePhysics() {
    if (this.tick >= this.nextBlinkAt && this.blinkPhase === 0) {
      this.blinkPhase = 1;
      this.nextBlinkAt = this.tick + 120 + Math.floor(Math.random() * 140);
    }
    if (this.blinkPhase > 0) {
      this.blinkPhase++;
      if (this.blinkPhase > 10) this.blinkPhase = 0;
    }

    if (this.tick % 95 === 0) {
      this.targetLookX = (Math.random() - 0.5) * 18;
      this.targetLookY = (Math.random() - 0.5) * 10;
    }
    this.lookX += (this.targetLookX - this.lookX) * 0.12;
    this.lookY += (this.targetLookY - this.lookY) * 0.12;
  }

  render() {
    const ctx = this.ctx;
    const t = FACE_THEMES[this.themeId] || FACE_THEMES.cyber_cyan;
    const W = 466;
    const H = 466;

    ctx.save();
    ctx.clearRect(0, 0, W, H);

    // Clip to round 466x466 AMOLED screen
    ctx.beginPath();
    ctx.arc(233, 233, 233, 0, Math.PI * 2);
    ctx.clip();

    // Deep background
    ctx.fillStyle = t.bgDeep;
    ctx.fillRect(0, 0, W, H);

    // Subtle HUD ring
    ctx.strokeStyle = t.hudRing;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(233, 233, 224, 0, Math.PI * 2);
    ctx.stroke();

    // 16 Orbiting HUD Nodes
    const spin = this.tick * 0.015;
    for (let i = 0; i < 16; i++) {
      const a = (i * Math.PI * 2) / 16 + spin * 0.25;
      const nx = 233 + Math.cos(a) * 218;
      const ny = 233 + Math.sin(a) * 218;
      const pulse = 0.35 + 0.65 * Math.max(0, Math.sin(this.tick * 0.08 + i * 0.7));
      ctx.globalAlpha = pulse;
      ctx.fillStyle = i % 4 === 0 ? t.eyeMain : t.hudDotDim;
      ctx.beginPath();
      ctx.arc(nx, ny, i % 4 === 0 ? 3.5 : 2.3, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;

    if (this.activeMenuKid >= 0 && this.kids[this.activeMenuKid]) {
      this.renderKidActivityOverlay(ctx, t, this.kids[this.activeMenuKid]);
      ctx.restore();
      return;
    }

    const hasKids = this.kids.length > 0;
    const centerY = hasKids ? 185 : 212;
    const scale = hasKids ? 1.0 : 1.08;

    this.renderEyesAndBrows(ctx, t, 233, centerY, scale);
    this.renderMouth(ctx, t, 233, centerY + 96 * scale, scale);

    if (hasKids) {
      this.renderKidButtons(ctx);
    } else {
      ctx.fillStyle = "rgba(148, 163, 184, 0.65)";
      ctx.font = "600 18px system-ui, sans-serif";
      ctx.textAlign = "center";
      ctx.fillText(`היי, אני ${this.robotName}!`, 233, 405);
    }

    ctx.restore();
  }

  renderEyesAndBrows(ctx, t, cx, cy, scale) {
    const emo = this.emotion;
    const blinkSquash =
      this.blinkPhase > 0
        ? Math.max(0.08, Math.abs(5 - this.blinkPhase) / 5)
        : 1.0;

    const spacing = 82 * scale;
    const baseW = 84 * scale;
    let leftH = 104 * scale * blinkSquash;
    let rightH = 104 * scale * blinkSquash;
    let browSlopeL = 0;
    let browSlopeR = 0;
    let browLift = 0;

    if (emo === "happy") {
      leftH *= 0.88;
      rightH *= 0.88;
      browLift = -6;
    } else if (emo === "laughing") {
      leftH *= 0.36;
      rightH *= 0.36;
      browLift = -10;
    } else if (emo === "surprised") {
      leftH *= 1.14;
      rightH *= 1.14;
      browLift = -14;
    } else if (emo === "winking") {
      rightH = 12 * scale;
      browSlopeR = 8;
      browLift = -5;
    } else if (emo === "cool") {
      leftH *= 0.62;
      rightH *= 0.62;
      browSlopeL = 7;
      browSlopeR = -7;
    } else if (emo === "thinking") {
      leftH *= 0.85;
      rightH *= 0.98;
      browSlopeL = -9;
      browSlopeR = 9;
    }

    const scanPhase = (Math.sin(this.tick * 0.06) + 1) * 0.5;

    const drawEye = (ex, ey, ew, eh) => {
      // Ambient outer halo
      ctx.save();
      ctx.globalAlpha = 0.26;
      ctx.fillStyle = t.eyeGlow;
      roundRect(ctx, ex - ew / 2 - 9, ey - eh / 2 - 9, ew + 18, eh + 18, 34);
      ctx.fill();

      // Main neon capsule
      ctx.globalAlpha = 1;
      ctx.shadowColor = t.eyeMain;
      ctx.shadowBlur = 18;
      ctx.fillStyle = t.eyeMain;
      roundRect(ctx, ex - ew / 2, ey - eh / 2, ew, eh, 28);
      ctx.fill();
      ctx.shadowBlur = 0;

      if (eh > 26) {
        // Bright inner core
        const cw = ew * 0.72;
        const ch = eh * 0.68;
        ctx.globalAlpha = 0.45;
        ctx.fillStyle = t.eyeCore;
        roundRect(
          ctx,
          ex - cw / 2 + this.lookX * 0.22,
          ey - ch / 2 + this.lookY * 0.22,
          cw,
          ch,
          20
        );
        ctx.fill();

        // Moving horizontal cyber scanline
        ctx.globalAlpha = 0.55;
        ctx.fillStyle = t.eyeScan;
        const sy = ey - eh * 0.34 + scanPhase * (eh * 0.68);
        roundRect(ctx, ex - ew * 0.38, sy, ew * 0.76, 4, 2);
        ctx.fill();

        // Specular glass highlights
        ctx.globalAlpha = 0.92;
        ctx.fillStyle = "#ffffff";
        ctx.beginPath();
        ctx.arc(ex - ew * 0.2, ey - eh * 0.24, 9 * scale, 0, Math.PI * 2);
        ctx.fill();
        ctx.globalAlpha = 0.65;
        ctx.beginPath();
        ctx.arc(ex + ew * 0.2, ey + eh * 0.22, 4.5 * scale, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.restore();
    };

    const lx = cx - spacing + this.lookX;
    const rx = cx + spacing + this.lookX;
    const ey = cy + this.lookY;

    drawEye(lx, ey, baseW, leftH);
    drawEye(rx, ey, baseW, rightH);

    // 5-segment eyebrows
    const drawBrow = (bx, by, slope) => {
      ctx.fillStyle = t.brow;
      for (let s = 0; s < 5; s++) {
        const u = (s - 2) / 2; // -1..1
        const sx = bx + u * 30 * scale;
        const arch = -Math.cos(u * 1.2) * 7;
        const sy = by + arch + u * slope;
        roundRect(ctx, sx - 8, sy - 4, 16, 7.5, 4);
        ctx.fill();
      }
    };

    drawBrow(lx, ey - 72 * scale + browLift, browSlopeL);
    drawBrow(rx, ey - 72 * scale + browLift, browSlopeR);
  }

  renderMouth(ctx, t, cx, my, scale) {
    const emo = this.emotion;
    const talkLvl =
      this.speakingLevel > 2
        ? this.speakingLevel
        : this.demoTalking
        ? 25 + 65 * Math.abs(Math.sin(this.tick * 0.24)) * Math.abs(Math.cos(this.tick * 0.11))
        : 0;

    ctx.save();
    for (let i = 0; i < 9; i++) {
      const u = (i - 4) / 4; // -1..1
      const x = cx + (i - 4) * 15 * scale;
      let h = 10 * scale;
      let yOff = 0;

      if (talkLvl > 2) {
        const wave =
          0.45 +
          0.55 * Math.abs(Math.sin(this.tick * 0.28 + i * 0.65));
        const bell = 1 - Math.abs(u) * 0.55;
        h = (8 + (talkLvl * 0.42) * wave * bell) * scale;
      } else if (emo === "happy" || emo === "winking" || emo === "cool") {
        yOff = (1 - u * u) * 12 * scale;
        h = (10 + (1 - Math.abs(u)) * 5) * scale;
      } else if (emo === "laughing") {
        yOff = (1 - u * u) * 15 * scale;
        h = (14 + (1 - Math.abs(u)) * 18) * scale;
      } else if (emo === "surprised") {
        const bell = Math.max(0, 1 - Math.abs(u) * 1.45);
        h = (8 + bell * 28) * scale;
      } else if (emo === "thinking") {
        yOff = u * 6 * scale;
        h = 9 * scale;
      }

      ctx.fillStyle = talkLvl > 2 ? t.mouthMain : t.mouthIdle;
      ctx.shadowColor = t.mouthMain;
      ctx.shadowBlur = talkLvl > 2 ? 12 : 6;
      roundRect(ctx, x - 5 * scale, my + yOff - h / 2, 10 * scale, h, 5 * scale);
      ctx.fill();
    }
    ctx.restore();
  }

  renderKidButtons(ctx) {
    this.pillRects = [];
    const n = Math.min(3, this.kids.length);
    const pillW = n === 1 ? 156 : n === 2 ? 132 : 98;
    const pillH = 44;
    const gap = n === 3 ? 8 : 14;
    const totalW = n * pillW + (n - 1) * gap;
    const startX = 233 - totalW / 2;
    const y = 358;

    for (let i = 0; i < n; i++) {
      const kid = this.kids[i];
      const x = startX + i * (pillW + gap);
      const col = kid.color || "#3b82f6";

      ctx.save();
      ctx.fillStyle = "rgba(15, 23, 42, 0.9)";
      ctx.strokeStyle = col;
      ctx.lineWidth = 2.5;
      ctx.shadowColor = col;
      ctx.shadowBlur = 10;
      roundRect(ctx, x, y, pillW, pillH, 22);
      ctx.fill();
      ctx.stroke();
      ctx.shadowBlur = 0;

      ctx.fillStyle = "#f8fafc";
      ctx.font = "700 18px system-ui, sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      const label = (kid.name || `ילד ${i + 1}`).slice(0, 10);
      ctx.fillText(label, x + pillW / 2, y + pillH / 2 + 1);
      ctx.restore();

      this.pillRects.push({ idx: i, x, y, w: pillW, h: pillH });
    }
  }

  renderKidActivityOverlay(ctx, t, kid) {
    const col = kid.color || t.eyeMain;
    ctx.fillStyle = "rgba(4, 9, 20, 0.94)";
    ctx.fillRect(0, 0, 466, 466);

    ctx.fillStyle = col;
    ctx.font = "700 26px system-ui, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(`${kid.name} — מה עושים?`, 233, 86);

    const acts = kid.activities || [];
    const boxW = 152;
    const boxH = 86;
    const gap = 16;
    const startX = 233 - boxW - gap / 2;
    const startY = 122;

    for (let i = 0; i < 4; i++) {
      const r = Math.floor(i / 2);
      const c = i % 2;
      const bx = startX + c * (boxW + gap);
      const by = startY + r * (boxH + gap);
      const lbl = acts[i]?.label || `פעילות ${i + 1}`;

      ctx.save();
      ctx.fillStyle = "rgba(15, 28, 52, 0.95)";
      ctx.strokeStyle = col;
      ctx.lineWidth = 2;
      roundRect(ctx, bx, by, boxW, boxH, 18);
      ctx.fill();
      ctx.stroke();

      ctx.fillStyle = "#f8fafc";
      ctx.font = "700 19px system-ui, sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(lbl.slice(0, 13), bx + boxW / 2, by + boxH / 2);
      ctx.restore();
    }

    // Back pill
    const bw = 128;
    const bh = 42;
    const bx = 233 - bw / 2;
    const by = 344;
    this.backRect = { x: bx, y: by, w: bw, h: bh };
    ctx.fillStyle = "rgba(30, 41, 59, 0.95)";
    ctx.strokeStyle = "rgba(148, 163, 184, 0.45)";
    ctx.lineWidth = 1.5;
    roundRect(ctx, bx, by, bw, bh, 21);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = "#e2e8f0";
    ctx.font = "600 17px system-ui, sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("חזרה לפנים", 233, by + bh / 2);
  }
}
