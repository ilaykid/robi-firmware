#ifndef ROBI_PORTAL_HTML_H_
#define ROBI_PORTAL_HTML_H_

namespace robi {

// Self-contained responsive Hebrew RTL Parent Portal HTML served directly from flash
inline constexpr const char kPortalHtml[] = R"ROBIHTML(<!DOCTYPE html>
<html lang="he" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HeyRobi — ממשק ניהול בית</title>
<style>
:root{--bg:#080d1a;--card:#11192e;--border:#1e2d4d;--cyan:#00e5ff;--txt:#f1f5f9;--muted:#94a3b8;--ok:#10b981;--err:#ef4444}
*{box-sizing:border-box;font-family:system-ui,-apple-system,'Segoe UI',sans-serif}
body{margin:0;background:radial-gradient(circle at top,#102042,#070b16 65%);color:var(--txt);padding:16px 12px 60px}
.wrap{max-width:680px;margin:0 auto}
header{text-align:center;margin:8px 0 20px}
h1{margin:0;font-size:1.65rem;background:linear-gradient(90deg,#00e5ff,#60a5fa,#c084fc);-webkit-background-clip:text;color:transparent}
.sub{color:var(--muted);font-size:.9rem;margin-top:4px}
.status-bar{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin-top:12px}
.pill{background:#0d1527;border:1px solid var(--border);border-radius:99px;padding:5px 12px;font-size:.82rem;color:#cbd5e1}
.topic-chip{display:inline-flex;align-items:center;gap:6px;background:#0f213d;border:1px solid var(--cyan);border-radius:99px;padding:5px 11px;font-size:.83rem;color:#e0f2fe;margin:4px 0 0 6px}
.topic-chip b{cursor:pointer;color:#fca5a5;font-size:.95rem}
.card{background:var(--card);border:1px solid var(--border);border-radius:18px;padding:18px;margin-bottom:14px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
.card h2{margin:0 0 12px;font-size:1.1rem;color:var(--cyan);display:flex;align-items:center;gap:8px}
label{display:block;font-size:.86rem;color:#cbd5e1;margin:10px 0 5px}
input[type=text],input[type=password],input[type=number],select,textarea{width:100%;background:#091022;border:1px solid #26385e;color:var(--txt);border-radius:11px;padding:10px 12px;font-size:.95rem}
textarea{min-height:68px;resize:vertical}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}
@media(max-width:520px){.row{grid-template-columns:1fr}}
.themes{display:grid;grid-template-columns:repeat(auto-fit,minmax(115px,1fr));gap:8px;margin-top:6px}
.theme-btn{background:#091022;border:2px solid #223254;border-radius:13px;padding:10px 8px;color:var(--txt);cursor:pointer;text-align:center;font-size:.85rem}
.theme-btn.active{border-color:var(--cyan);background:#0f213d}
.dot{display:inline-block;width:16px;height:16px;border-radius:50%;vertical-align:middle;margin-left:5px}
.kid-box{background:#0a1224;border:1px solid #223357;border-radius:14px;padding:13px;margin-top:10px}
.kid-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.acts-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
@media(max-width:520px){.acts-grid{grid-template-columns:1fr}}
.act-item{background:#101b33;border:1px solid #223357;border-radius:10px;padding:8px}
button{cursor:pointer;border:none;border-radius:12px;padding:11px 16px;font-weight:600;font-size:.95rem}
.btn-main{width:100%;background:linear-gradient(90deg,#0284c7,#00c6ff);color:#fff;font-size:1.05rem;padding:14px;margin-top:6px}
.btn-sec{background:#1e293b;color:#e2e8f0;border:1px solid #334155;padding:8px 13px;font-size:.85rem}
.btn-danger{background:#3b1219;color:#fca5a5;border:1px solid #7f1d1d;padding:7px 11px;font-size:.8rem}
.emo-row{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#064e3b;border:1px solid #10b981;color:#fff;padding:10px 20px;border-radius:99px;display:none;z-index:99}
.mem-list{margin:8px 0;padding-right:18px;color:#cbd5e1;font-size:.88rem}
.q-box{background:#091022;border:1px solid #223357;border-radius:13px;padding:12px;margin-bottom:10px}
.q-bar{height:10px;background:#16223b;border-radius:99px;overflow:hidden;margin:7px 0}
.q-fill{height:100%;background:linear-gradient(90deg,#10b981,#00e5ff);border-radius:99px;transition:width .35s}
.m-card{background:#0a1224;border:1px solid #223357;border-radius:13px;padding:11px 13px;margin-top:8px;display:flex;flex-direction:column;gap:5px}
.m-card.live{border-color:#10b981;box-shadow:0 0 14px rgba(16,185,129,.22)}
.m-head{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px}
.m-badge{font-size:.76rem;padding:3px 9px;border-radius:99px;font-weight:600}
.m-stats{display:flex;flex-wrap:wrap;gap:10px;font-size:.81rem;color:#94a3b8}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>🤖 HeyRobi — ניהול בית</h1>
    <div class="sub">ניהול אלחוטי מקומי — כל השינויים מתעדכנים מיד על המכשיר</div>
    <div class="status-bar" id="statusBar"><span class="pill">טוען נתונים...</span></div>
  </header>

  <div class="card">
    <h2>🔑 מפתח Gemini, מודל וקול</h2>
    <label>מפתח Gemini API <span id="keyHint" style="color:#10b981;font-size:.8rem"></span></label>
    <input type="password" id="geminiKey" dir="ltr" placeholder="השאר ריק כדי לשמור על המפתח הקיים, או הדבק מפתח חדש (AIza...)">
    <div class="row">
      <div>
        <label>שם הרובוט</label>
        <input type="text" id="botName" value="רובי">
      </div>
      <div>
        <label>שפת דיבור ראשית</label>
        <select id="language">
          <option value="he-IL">עברית (מומלץ)</option>
          <option value="en-US">English</option>
        </select>
      </div>
    </div>
    <label>מודל Gemini Live מועדף</label>
    <select id="preferredModel">
      <option value="auto">אוטומטי חכם — Gemini 3.8 Live ראשי + גיבוי אוטומטי (מומלץ!)</option>
      <option value="gemini-3.8-live">Gemini 3.8 Live — הדור החדש עם עברית מובנית (he-IL)</option>
      <option value="gemini-2.5-flash-native-audio-preview-12-2025">Gemini 2.5 Flash Native Audio — מהיר ויציב</option>
      <option value="gemini-2.0-flash-live-001">Gemini 2.0 Flash Live — מודל גיבוי קלאסי</option>
    </select>
    <label>הקול של רובי</label>
    <select id="voice">
      <option value="Puck">Puck — שובב, אנרגטי וזורם (מומלץ!)</option>
      <option value="Fenrir">Fenrir — הרפתקני, חם ועמוק</option>
      <option value="Zephyr">Zephyr — רגוע, נעים וידידותי</option>
      <option value="Leda">Leda — חמה, רכה וסיפורית</option>
      <option value="Aoede">Aoede — קלילה, שמחה ומוזיקלית</option>
      <option value="Kore">Kore — בהירה, סקרנית ונעימה</option>
      <option value="Charon">Charon — חכם, שקול ומסתורי</option>
      <option value="Achird">Achird — צעיר, סקרן ונלהב</option>
    </select>
  </div>

  <div class="card">
    <h2>📊 מודלי Gemini Live ומעקב שימוש מקומי (במכשיר)</h2>
    <div id="quotaBox" class="q-box">טוען נתוני מעקב שימוש מקומי...</div>
    <div id="modelsList"></div>
    <div style="display:flex;justify-content:space-between;align-items:center;margin-top:10px;flex-wrap:wrap;gap:8px">
      <span style="font-size:.79rem;color:var(--muted)" id="usageDayLbl">הערכה מקומית במכשיר • המכסה האמיתית מול Google מתאפסת אוטומטית אצלם אחת ליממה</span>
      <button type="button" class="btn-sec" onclick="resetUsage()">🔄 רענן מודלים ואפס מונה מקומי (במכשיר בלבד)</button>
    </div>
  </div>

  <div class="card">
    <h2>🎨 סגנון פרצוף וערכת צבעים</h2>
    <label>צורת הפנים של רובי:</label>
    <div class="themes" id="styleGrid"></div>
    <label style="margin-top:12px">צבע הניאון:</label>
    <div class="themes" id="themeGrid"></div>
    <label style="margin-top:12px">בדיקת הבעה בשידור חי על המסך של רובי:</label>
    <div class="emo-row">
      <button type="button" class="btn-sec" onclick="testEmo('happy')">😄 שמח</button>
      <button type="button" class="btn-sec" onclick="testEmo('laughing')">😂 צוחק</button>
      <button type="button" class="btn-sec" onclick="testEmo('surprised')">🤩 מופתע</button>
      <button type="button" class="btn-sec" onclick="testEmo('winking')">😉 קורץ</button>
      <button type="button" class="btn-sec" onclick="testEmo('cool')">😎 מגניב</button>
      <button type="button" class="btn-sec" onclick="testEmo('loving')">🥰 אוהב</button>
      <button type="button" class="btn-sec" onclick="testEmo('thinking')">🤔 חושב</button>
    </div>
  </div>

  <div class="card">
    <h2>💬 נושאי שיחה אישיים שההורים מוסיפים</h2>
    <div style="color:var(--muted);font-size:.84rem;margin-bottom:8px">כתבו חופשי כל נושא שתרצו שרובי ישלב בשיחות, בחידות ובסיפורים (למשל: כדורסל, שחמט, הכנה לכיתה ג', מיתולוגיה יוונית, גיטרה...)</div>
    <div style="display:flex;gap:8px">
      <input type="text" id="newTopicInp" placeholder="הקלידו נושא שיחה משלכם..." onkeydown="if(event.key==='Enter'){event.preventDefault();addTopic();}">
      <button type="button" class="btn-sec" onclick="addTopic()">+ הוסף</button>
    </div>
    <div id="topicsWrap" style="margin-top:8px"></div>
  </div>

  <div class="card">
    <h2>👧👦 ילדים וכפתורי נושאים על המסך (עד 3 ילדים)</h2>
    <div style="color:var(--muted);font-size:.84rem;margin-bottom:8px">אפשר להשאיר ללא ילדים (לחיצה על הפנים של רובי לשיחה חופשית), או להוסיף עד 3 ילדים עם תחומי עניין וכפתורי פעילות מותאמים.</div>
    <div id="kidsWrap"></div>
    <button type="button" class="btn-sec" id="addKidBtn" onclick="addKid()" style="margin-top:10px">+ הוסף ילד/ה</button>
  </div>

  <div class="card">
    <h2>📝 דגשים להורים וזיכרון</h2>
    <label>הנחיות אישיות לרובי (אופציונלי)</label>
    <textarea id="customPrompt" placeholder="למשל: עודד חשיבה עצמאית, הימנע ממסכים לפני השינה, שלב חידות משעשעות..."></textarea>
    <label style="margin-top:12px">זיכרונות שרובי שמר משיחות אחרונות:</label>
    <ul class="mem-list" id="memList"><li>אין זיכרונות שמורים עדיין</li></ul>
    <button type="button" class="btn-danger" onclick="clearMem()">נקה זיכרונות שיחה</button>
  </div>

  <div class="card">
    <h2>🔊 עוצמת שמע, בהירות ומערכת</h2>
    <div class="row">
      <div>
        <label>עוצמת רמקול: <span id="volLbl">80</span>%</label>
        <input type="range" id="volRange" dir="ltr" min="35" max="100" value="80" style="width:100%" onchange="sendCtrl({volume:+this.value});document.getElementById('volLbl').textContent=this.value">
      </div>
      <div>
        <label>בהירות מסך: <span id="briLbl">85</span>%</label>
        <input type="range" id="briRange" dir="ltr" min="10" max="100" value="85" style="width:100%" onchange="sendCtrl({brightness:+this.value});document.getElementById('briLbl').textContent=this.value">
      </div>
    </div>
  </div>

  <button type="button" class="btn-main" onclick="saveAll()">💾 שמור והחל מיד על רובי</button>
</div>
<div class="toast" id="toast"></div>

<script>
const STYLES=[
  {id:'cyber_visor',name:'קפסולות סייבר'},
  {id:'round_kawaii',name:'עגול חמוד'},
  {id:'retro_pixel',name:'רטרו פיקסל'},
  {id:'visor_cyclops',name:'משקף סייבורג'},
  {id:'owl_mech',name:'עדשות מכניות'}
];
const THEMES=[
  {id:'cyber_cyan',name:'סייבר טורקיז',hex:'#00E5FF'},
  {id:'solar_gold',name:'שמש זהובה',hex:'#FFB703'},
  {id:'emerald_matrix',name:'מטריקס ירוק',hex:'#10B981'},
  {id:'cosmic_violet',name:'גלקסיה סגולה',hex:'#A855F7'},
  {id:'coral_cute',name:'קורל חמוד',hex:'#FF5C8A'}
];
const DEF_ACTS=[
  {label:'חשבון',prompt:'בוא נתרגל חשבון בכיף'},
  {label:'אנגלית',prompt:'בוא נתרגל אנגלית בכיף'},
  {label:'ידע כללי',prompt:'ספר לי משהו מרתק ומפתיע במדע, חלל או חיות!'},
  {label:'סיפור',prompt:'בוא נמשיך או נמציא יחד סיפור הרפתקאות'}
];
let state={theme:'cyber_cyan',face_style:'cyber_visor',custom_topics:[],kids:[]};

function showToast(msg,err){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.style.background=err?'#7f1d1d':'#064e3b';
  t.style.borderColor=err?'#ef4444':'#10b981';
  t.style.display='block';
  setTimeout(()=>t.style.display='none',2800);
}

function renderStyles(){
  const g=document.getElementById('styleGrid');
  g.innerHTML='';
  STYLES.forEach(st=>{
    const b=document.createElement('div');
    b.className='theme-btn'+(state.face_style===st.id?' active':'');
    b.textContent=st.name;
    b.onclick=()=>{state.face_style=st.id;renderStyles();};
    g.appendChild(b);
  });
}

function renderThemes(){
  const g=document.getElementById('themeGrid');
  g.innerHTML='';
  THEMES.forEach(th=>{
    const b=document.createElement('div');
    b.className='theme-btn'+(state.theme===th.id?' active':'');
    b.innerHTML=`<span class="dot" style="background:${th.hex};box-shadow:0 0 10px ${th.hex}"></span>${th.name}`;
    b.onclick=()=>{state.theme=th.id;renderThemes();};
    g.appendChild(b);
  });
}

function renderTopics(){
  const w=document.getElementById('topicsWrap');
  w.innerHTML=(state.custom_topics||[]).map((t,i)=>`<span class="topic-chip">${esc(t)} <b onclick="removeTopic(${i})">×</b></span>`).join('');
}
function addTopic(){
  const inp=document.getElementById('newTopicInp');
  const v=inp.value.trim();
  if(!v||(state.custom_topics||[]).length>=16)return;
  if(!state.custom_topics.includes(v))state.custom_topics.push(v);
  inp.value='';
  renderTopics();
}
function removeTopic(i){state.custom_topics.splice(i,1);renderTopics();}

function esc(s){return (s||'').replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;');}

function renderKids(){
  const w=document.getElementById('kidsWrap');
  w.innerHTML='';
  state.kids.forEach((k,idx)=>{
    const acts=(k.activities&&k.activities.length===4)?k.activities:JSON.parse(JSON.stringify(DEF_ACTS));
    k.activities=acts;
    const d=document.createElement('div');
    d.className='kid-box';
    d.innerHTML=`
      <div class="kid-head">
        <strong>ילד/ה #${idx+1}</strong>
        <button type="button" class="btn-danger" onclick="removeKid(${idx})">הסר</button>
      </div>
      <div class="row">
        <div><label>שם</label><input type="text" value="${esc(k.name)}" oninput="state.kids[${idx}].name=this.value"></div>
        <div style="display:grid;grid-template-columns:1fr 90px;gap:8px">
          <div><label>גיל</label><input type="number" min="3" max="18" value="${k.age||8}" oninput="state.kids[${idx}].age=+this.value"></div>
          <div><label>צבע כפתור</label><input type="color" value="${esc(k.color||'#0284C7')}" style="height:41px;width:100%;border:none;background:none" oninput="state.kids[${idx}].color=this.value"></div>
        </div>
      </div>
      <label>תחומי עניין ואהבות</label>
      <input type="text" value="${esc(k.interests)}" placeholder="למשל: חלל, כדורגל, דינוזאורים, מיינקראפט..." oninput="state.kids[${idx}].interests=this.value">
      <label>4 כפתורי פעילות בתפריט של הילד/ה:</label>
      <div class="acts-grid">
        ${acts.map((a,ai)=>`
          <div class="act-item">
            <input type="text" value="${esc(a.label)}" placeholder="כותרת כפתור" style="margin-bottom:5px;padding:7px" oninput="state.kids[${idx}].activities[${ai}].label=this.value">
            <input type="text" value="${esc(a.prompt)}" placeholder="מה לבקש מרובי" style="font-size:.82rem;padding:7px" oninput="state.kids[${idx}].activities[${ai}].prompt=this.value">
          </div>`).join('')}
      </div>`;
    w.appendChild(d);
  });
  document.getElementById('addKidBtn').style.display=state.kids.length>=3?'none':'inline-block';
}

function addKid(){
  if(state.kids.length>=3)return;
  const cols=['#0284C7','#EA580C','#10B981'];
  state.kids.push({name:'',age:8,color:cols[state.kids.length],interests:'',activities:JSON.parse(JSON.stringify(DEF_ACTS))});
  renderKids();
}
function removeKid(i){state.kids.splice(i,1);renderKids();}

async function sendCtrl(body){
  try{await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json','X-Robi-Token':'1'},body:JSON.stringify(body)});}catch(e){}
}
function testEmo(e){sendCtrl({emotion:e});}

async function clearMem(){
  await fetch('/api/memory/clear',{method:'POST',headers:{'X-Robi-Token':'1'}});
  document.getElementById('memList').innerHTML='<li>אין זיכרונות שמורים עדיין</li>';
  showToast('הזיכרונות נוקו');
}

async function resetUsage(){
  await sendCtrl({reset_usage:true});
  showToast('מונה המעקב המקומי במכשיר אופס וחיבור המודלים רוענן (המכסה בצד של Google אינה מושפעת)');
  refreshStatusOnly();
}

function fmtAudioSec(sec){
  sec=+sec||0;
  const m=Math.floor(sec/60),s=sec%60;
  return m>0?`${m} דק' ו-${s} שנ'`:`${s} שנ'`;
}

function renderModelsAndQuota(st){
  const q=st.daily_quota||{used_sessions:0,max_sessions:500,used_turns:0,used_audio_sec:0,max_audio_sec:2700,used_tokens:0,max_tokens:1000000};
  const pctAudio=Math.min(100,Math.round((q.used_audio_sec*100)/(q.max_audio_sec||2700)));
  const pctSess=Math.min(100,Math.round((q.used_sessions*100)/(q.max_sessions||500)));
  const pctTok=Math.min(100,Math.round((q.used_tokens*100)/(q.max_tokens||1000000)));
  const topPct=Math.max(pctAudio,pctSess,pctTok);
  const barCol=topPct>80?'linear-gradient(90deg,#f59e0b,#ef4444)':'linear-gradient(90deg,#10b981,#00e5ff)';

  document.getElementById('quotaBox').innerHTML=`
    <div style="display:flex;justify-content:space-between;font-size:.88rem;font-weight:600">
      <span>🎙️ זמן שיחה נטו שנמדד במכשיר היום: ${fmtAudioSec(q.used_audio_sec)} (מתוך הערכת ~45 דק' חינם ב-Google)</span>
      <span dir="ltr" style="color:var(--cyan)">${topPct}%</span>
    </div>
    <div class="q-bar"><div class="q-fill" style="width:${Math.max(2,topPct)}%;background:${barCol}"></div></div>
    <div class="m-stats" style="color:#cbd5e1;margin-top:4px">
      <span>🔌 סשנים שנמדדו במכשיר: <b>${q.used_sessions} / ${q.max_sessions}</b></span>
      <span>💬 חילופי דברים: <b>${q.used_turns}</b></span>
      <span>🔢 טוקנים משוערים (ספירה מקומית): <b dir="ltr">${(q.used_tokens||0).toLocaleString()} / 1,000,000</b></span>
    </div>`;

  const META={
    'gemini-3.8-live':{title:'Gemini 3.8 Live',desc:'הדור החדש — עברית מובנית ברמת הפרוטוקול (he-IL)'},
    'gemini-2.5-flash-native-audio-preview-12-2025':{title:'Gemini 2.5 Flash Native Audio',desc:'מודל אודיו ילידי מהיר ויציב'},
    'gemini-2.0-flash-live-001':{title:'Gemini 2.0 Flash Live',desc:'מודל גיבוי רב-תכליתי יציב'}
  };
  const models=st.models||[];
  document.getElementById('modelsList').innerHTML=models.map(m=>{
    const info=META[m.id]||{title:m.id,desc:''};
    let badge='<span class="m-badge" style="background:#1e293b;color:#cbd5e1">⚡ זמין ומוכן</span>';
    if(m.active){
      badge='<span class="m-badge" style="background:#064e3b;color:#34d399;border:1px solid #10b981">🟢 פעיל בשיחה כעת</span>';
    }else if(m.cooldown_sec>0){
      badge=`<span class="m-badge" style="background:#3b1219;color:#fca5a5">🔄 עבר לגיבוי (${m.cooldown_sec} שנ')</span>`;
    }else if(m.last_used){
      badge='<span class="m-badge" style="background:#0f213d;color:#38bdf8;border:1px solid #0284c7">✅ אחרון בשימוש</span>';
    }
    return `<div class="m-card${m.active?' live':''}">
      <div class="m-head">
        <div>
          <strong dir="ltr" style="color:#f8fafc">${esc(info.title)}</strong>
          <div style="font-size:.78rem;color:var(--muted)">${esc(info.desc)}</div>
        </div>
        ${badge}
      </div>
      <div class="m-stats">
        <span>🔌 סשנים: <b>${m.sessions||0}</b></span>
        <span>💬 תורות שיחה: <b>${m.turns||0}</b></span>
        <span>🎙️ זמן דיבור: <b>${fmtAudioSec(m.audio_sec||0)}</b></span>
        <span>🔢 טוקנים: <b dir="ltr">${(m.tokens||0).toLocaleString()}</b></span>
      </div>
    </div>`;
  }).join('');

  if(st.usage_day){
    document.getElementById('usageDayLbl').textContent=`תאריך מונה מקומי במכשיר: ${st.usage_day} • המכסה האמיתית מנוהלת בשרתי Google ומתאפסת אצלם אוטומטית`;
  }
}

async function saveAll(){
  const keyVal=document.getElementById('geminiKey').value.trim();
  const payload={
    bot_name:document.getElementById('botName').value.trim()||'רובי',
    gemini_key:keyVal?keyVal:'__KEEP__',
    preferred_model:document.getElementById('preferredModel').value,
    voice:document.getElementById('voice').value,
    language:document.getElementById('language').value,
    theme:state.theme,
    face_style:state.face_style,
    volume:+document.getElementById('volRange').value,
    brightness:+document.getElementById('briRange').value,
    custom_topics:state.custom_topics||[],
    custom_prompt:document.getElementById('customPrompt').value.trim(),
    kids:state.kids.filter(k=>k.name&&k.name.trim())
  };
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json','X-Robi-Token':'1'},body:JSON.stringify(payload)});
    if(!r.ok)throw new Error('שגיאה בשמירה');
    document.getElementById('geminiKey').value='';
    showToast('ההגדרות נשמרו והוחלו על רובי!');
    loadAll();
  }catch(e){showToast('שגיאה בשמירה: '+e.message,true);}
}

function getLocalIsoDay(){
  const d=new Date();
  return d.getFullYear()+'-'+String(d.getMonth()+1).padStart(2,'0')+'-'+String(d.getDate()).padStart(2,'0');
}

async function refreshStatusOnly(){
  try{
    const sr=await fetch('/api/status?day='+encodeURIComponent(getLocalIsoDay()));
    const st=await sr.json();
    const actMod=st.active_model||st.last_model||'מוכן';
    document.getElementById('statusBar').innerHTML=`
      <span class="pill">📶 ${esc(st.ssid||'WiFi')} (${st.rssi||0} dBm)</span>
      <span class="pill" dir="ltr">🌐 ${esc(st.ip||'robi.local')}</span>
      <span class="pill" dir="ltr">🤖 ${esc(actMod)}</span>
      <span class="pill" dir="ltr">🔋 ${st.battery??100}% ${st.charging?'⚡':''}</span>`;
    renderModelsAndQuota(st);
    const ml=document.getElementById('memList');
    if(ml){
      ml.innerHTML=(st.memories&&st.memories.length)?st.memories.map(m=>`<li>${esc(m)}</li>`).join(''):'<li>אין זיכרונות שמורים עדיין</li>';
    }
  }catch(e){}
}

async function loadAll(){
  try{
    const [cr,sr]=await Promise.all([fetch('/api/config'),fetch('/api/status?day='+encodeURIComponent(getLocalIsoDay()))]);
    const cfg=await cr.json();
    const st=await sr.json();
    document.getElementById('botName').value=cfg.bot_name||'רובי';
    document.getElementById('preferredModel').value=cfg.preferred_model||'auto';
    document.getElementById('voice').value=cfg.voice||'Puck';
    document.getElementById('language').value=cfg.language||'he-IL';
    document.getElementById('customPrompt').value=cfg.custom_prompt||'';
    document.getElementById('keyHint').textContent=cfg.has_key?`(מוגדר: ${cfg.key_hint})`:'(טרם הוגדר מפתח!)';
    state.theme=cfg.theme||'cyber_cyan';
    state.face_style=cfg.face_style||'cyber_visor';
    state.custom_topics=cfg.custom_topics||[];
    state.kids=cfg.kids||[];
    renderStyles();
    renderThemes();
    renderTopics();
    renderKids();

    const actMod=st.active_model||st.last_model||'מוכן';
    document.getElementById('statusBar').innerHTML=`
      <span class="pill">📶 ${esc(st.ssid||'WiFi')} (${st.rssi||0} dBm)</span>
      <span class="pill" dir="ltr">🌐 ${esc(st.ip||'robi.local')}</span>
      <span class="pill" dir="ltr">🤖 ${esc(actMod)}</span>
      <span class="pill" dir="ltr">🔋 ${st.battery??100}% ${st.charging?'⚡':''}</span>`;
    renderModelsAndQuota(st);
    if(st.volume!==undefined){document.getElementById('volRange').value=st.volume;document.getElementById('volLbl').textContent=st.volume;}
    if(st.brightness!==undefined){document.getElementById('briRange').value=st.brightness;document.getElementById('briLbl').textContent=st.brightness;}
    const ml=document.getElementById('memList');
    if(st.memories&&st.memories.length){
      ml.innerHTML=st.memories.map(m=>`<li>${esc(m)}</li>`).join('');
    }else{
      ml.innerHTML='<li>אין זיכרונות שמורים עדיין</li>';
    }
  }catch(e){}
}
loadAll();
setInterval(refreshStatusOnly,5000);
</script>
</body>
</html>)ROBIHTML";

}  // namespace robi

#endif  // ROBI_PORTAL_HTML_H_
