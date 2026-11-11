#include "robi_http_portal.h"
#include "robi_config.h"

#include "application.h"
#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "display.h"
#include "wifi_board.h"

#include <arpa/inet.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#define TAG "RobiPortal"

namespace {

// Self-contained responsive Hebrew RTL Parent Portal HTML served directly from flash
const char kPortalHtml[] = R"ROBIHTML(<!DOCTYPE html>
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
    <h2>🔑 מפתח Gemini וקול</h2>
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
        <label>עוצמת רמקול: <span id="volLbl">75</span>%</label>
        <input type="range" id="volRange" min="0" max="100" value="75" style="width:100%" onchange="sendCtrl({volume:+this.value});document.getElementById('volLbl').textContent=this.value">
      </div>
      <div>
        <label>בהירות מסך: <span id="briLbl">85</span>%</label>
        <input type="range" id="briRange" min="10" max="100" value="85" style="width:100%" onchange="sendCtrl({brightness:+this.value});document.getElementById('briLbl').textContent=this.value">
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
  try{await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});}catch(e){}
}
function testEmo(e){sendCtrl({emotion:e});}

async function clearMem(){
  await fetch('/api/memory/clear',{method:'POST'});
  document.getElementById('memList').innerHTML='<li>אין זיכרונות שמורים עדיין</li>';
  showToast('הזיכרונות נוקו');
}

async function saveAll(){
  const keyVal=document.getElementById('geminiKey').value.trim();
  const payload={
    bot_name:document.getElementById('botName').value.trim()||'רובי',
    gemini_key:keyVal?keyVal:'__KEEP__',
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
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    if(!r.ok)throw new Error('שגיאה בשמירה');
    document.getElementById('geminiKey').value='';
    showToast('ההגדרות נשמרו והוחלו על רובי!');
    loadAll();
  }catch(e){showToast('שגיאה בשמירה: '+e.message,true);}
}

async function loadAll(){
  try{
    const [cr,sr]=await Promise.all([fetch('/api/config'),fetch('/api/status')]);
    const cfg=await cr.json();
    const st=await sr.json();
    document.getElementById('botName').value=cfg.bot_name||'רובי';
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

    document.getElementById('statusBar').innerHTML=`
      <span class="pill">📶 ${esc(st.ssid||'WiFi')} (${st.rssi||0} dBm)</span>
      <span class="pill">🌐 ${esc(st.ip||'')}</span>
      <span class="pill">🔋 ${st.battery??100}% ${st.charging?'⚡':''}</span>`;
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
</script>
</body>
</html>)ROBIHTML";

esp_err_t HandleIndexGet(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kPortalHtml, sizeof(kPortalHtml) - 1);
}

esp_err_t HandleConfigGet(httpd_req_t* req) {
    std::string json = RobiConfig::GetInstance().ExportJson(/*mask_api_key=*/true);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, json.data(), json.size());
}

esp_err_t ReadBody(httpd_req_t* req, std::string& out, size_t max_len) {
    if (req->content_len == 0 || req->content_len > max_len) {
        return ESP_FAIL;
    }
    out.resize(req->content_len);
    size_t received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, out.data() + received, req->content_len - received);
        if (r <= 0) return ESP_FAIL;
        received += static_cast<size_t>(r);
    }
    return ESP_OK;
}

esp_err_t HandleConfigPost(httpd_req_t* req) {
    std::string body;
    if (ReadBody(req, body, RobiConfig::kMaxConfigJsonBytes) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    std::string err;
    if (!RobiConfig::GetInstance().UpdateFromJson(body, false, err)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err.c_str());
        return ESP_FAIL;
    }

    // Reset active voice channel if idle/listening so next turn uses updated voice/prompt
    Application::GetInstance().Schedule([]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateIdle) {
            app.DismissAlert();
        }
    });

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

esp_err_t HandleStatusGet(httpd_req_t* req) {
    auto& board = Board::GetInstance();
    auto& wifi = WifiManager::GetInstance();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", wifi.GetSsid().c_str());
    cJSON_AddStringToObject(root, "ip", wifi.GetIpAddress().c_str());
    cJSON_AddNumberToObject(root, "rssi", wifi.GetRssi());

    int bat = 100;
    bool chg = false, dis = false;
    if (board.GetBatteryLevel(bat, chg, dis)) {
        cJSON_AddNumberToObject(root, "battery", bat);
        cJSON_AddBoolToObject(root, "charging", chg);
    }
    if (auto* codec = board.GetAudioCodec()) {
        cJSON_AddNumberToObject(root, "volume", codec->output_volume());
    }
    if (auto* bl = board.GetBacklight()) {
        cJSON_AddNumberToObject(root, "brightness", bl->brightness());
    }

    cJSON* mems = cJSON_CreateArray();
    for (const auto& m : RobiConfig::GetInstance().GetMemoryNotes()) {
        cJSON_AddItemToArray(mems, cJSON_CreateString(m.c_str()));
    }
    cJSON_AddItemToObject(root, "memories", mems);

    char* s = cJSON_PrintUnformatted(root);
    std::string out = s ? s : "{}";
    if (s) cJSON_free(s);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, out.data(), out.size());
}

esp_err_t HandleMemoryClearPost(httpd_req_t* req) {
    (void)req;
    RobiConfig::GetInstance().ClearMemoryNotes();
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

esp_err_t HandleControlPost(httpd_req_t* req) {
    std::string body;
    if (ReadBody(req, body, 1024) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_FAIL;
    }
    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    const cJSON* vol = cJSON_GetObjectItemCaseSensitive(root, "volume");
    if (cJSON_IsNumber(vol)) {
        int v = std::clamp(vol->valueint, 0, 100);
        if (auto* codec = Board::GetInstance().GetAudioCodec()) {
            codec->SetOutputVolume(v);
        }
    }
    const cJSON* bri = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (cJSON_IsNumber(bri)) {
        int b = std::clamp(bri->valueint, 10, 100);
        if (auto* bl = Board::GetInstance().GetBacklight()) {
            bl->SetBrightness(static_cast<uint8_t>(b), true);
        }
    }
    const cJSON* emo = cJSON_GetObjectItemCaseSensitive(root, "emotion");
    if (cJSON_IsString(emo) && emo->valuestring != nullptr) {
        std::string e_str = emo->valuestring;
        Application::GetInstance().Schedule([e_str]() {
            if (auto* disp = Board::GetInstance().GetDisplay()) {
                disp->SetEmotion(e_str.c_str());
            }
        });
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "reboot"))) {
        Application::GetInstance().Schedule([]() { Application::GetInstance().Reboot(); });
    }
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const char* ok = "{\"ok\":true}";
    return httpd_resp_send(req, ok, std::strlen(ok));
}

// Case-insensitive check whether any DNS question in pkt asks for "robi.local" or "robi"
bool ParseMdnsRobiLocalQuery(const uint8_t* pkt, size_t len, uint16_t& qtype_out) {
    if (len < 22) return false;
    uint16_t flags = (static_cast<uint16_t>(pkt[2]) << 8) | pkt[3];
    if (flags & 0x8000) return false;  // Ignore responses
    uint16_t qdcount = (static_cast<uint16_t>(pkt[4]) << 8) | pkt[5];
    if (qdcount == 0) return false;

    // Scan packet questions for label "robi" followed by either "local" or end-of-name (LLMNR/mDNS)
    size_t pos = 12;
    qtype_out = 1;
    bool matched = false;
    for (uint16_t q = 0; q < qdcount && pos < len; ++q) {
        size_t name_start = pos;
        bool first_is_robi = false;
        bool second_is_local_or_end = false;
        int label_idx = 0;
        bool jumped = false;
        size_t cur = pos;
        int hops = 0;

        while (cur < len && hops < 16) {
            uint8_t llen = pkt[cur];
            if (llen == 0) {
                if (label_idx == 1 && first_is_robi) {
                    second_is_local_or_end = true;
                }
                if (!jumped) pos = cur + 1;
                break;
            }
            if ((llen & 0xC0) == 0xC0) {
                if (cur + 1 >= len) return matched;
                uint16_t ptr = ((static_cast<uint16_t>(llen & 0x3F) << 8) | pkt[cur + 1]);
                if (!jumped) pos = cur + 2;
                jumped = true;
                cur = ptr;
                ++hops;
                continue;
            }
            if (cur + 1 + llen > len) return matched;
            if (label_idx == 0 && llen == 4) {
                first_is_robi = (std::tolower(pkt[cur + 1]) == 'r' &&
                                 std::tolower(pkt[cur + 2]) == 'o' &&
                                 std::tolower(pkt[cur + 3]) == 'b' &&
                                 std::tolower(pkt[cur + 4]) == 'i');
            } else if (label_idx == 1 && first_is_robi && llen == 5) {
                second_is_local_or_end = (std::tolower(pkt[cur + 1]) == 'l' &&
                                          std::tolower(pkt[cur + 2]) == 'o' &&
                                          std::tolower(pkt[cur + 3]) == 'c' &&
                                          std::tolower(pkt[cur + 4]) == 'a' &&
                                          std::tolower(pkt[cur + 5]) == 'l');
            }
            cur += 1 + llen;
            ++label_idx;
        }
        (void)name_start;
        if (pos + 4 > len) break;
        uint16_t qtype = (static_cast<uint16_t>(pkt[pos]) << 8) | pkt[pos + 1];
        pos += 4;  // skip QTYPE + QCLASS

        if (first_is_robi && second_is_local_or_end) {
            matched = true;
            if (qtype == 1 || qtype == 255) {
                qtype_out = 1;
                return true;
            }
            qtype_out = qtype;
        }
    }
    return matched;
}

}  // namespace

RobiHttpPortal& RobiHttpPortal::GetInstance() {
    static RobiHttpPortal instance;
    return instance;
}

void RobiHttpPortal::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (server_ != nullptr) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) == ESP_OK) {
        static const httpd_uri_t kUris[] = {
            {"/", HTTP_GET, HandleIndexGet, nullptr},
            {"/api/config", HTTP_GET, HandleConfigGet, nullptr},
            {"/api/config", HTTP_POST, HandleConfigPost, nullptr},
            {"/api/status", HTTP_GET, HandleStatusGet, nullptr},
            {"/api/memory/clear", HTTP_POST, HandleMemoryClearPost, nullptr},
            {"/api/control", HTTP_POST, HandleControlPost, nullptr},
        };
        for (const auto& u : kUris) {
            httpd_register_uri_handler(server_, &u);
        }
        std::string ip = WifiManager::GetInstance().GetIpAddress();
        ESP_LOGI(TAG, "Robi Parent Portal active at http://robi.local and http://%s", ip.c_str());
    } else {
        ESP_LOGW(TAG, "Failed to start HTTP server on port 80");
        server_ = nullptr;
    }

    if (!mdns_running_.load()) {
        mdns_running_.store(true);
        xTaskCreate(
            [](void* arg) {
                static_cast<RobiHttpPortal*>(arg)->MdnsResponderLoop();
                vTaskDelete(nullptr);
            },
            "robi_mdns", 4096, this, 2, &mdns_task_);
    }
}

void RobiHttpPortal::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    mdns_running_.store(false);
    if (mdns_sock_ >= 0) {
        shutdown(mdns_sock_, SHUT_RDWR);
        close(mdns_sock_);
        mdns_sock_ = -1;
    }
    mdns_task_ = nullptr;

    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
        ESP_LOGI(TAG, "Robi Parent Portal stopped");
    }
}

void RobiHttpPortal::MdnsResponderLoop() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        mdns_running_.store(false);
        return;
    }
    mdns_sock_ = sock;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(5353);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        close(sock);
        mdns_sock_ = -1;
        mdns_running_.store(false);
        return;
    }

    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    timeval tv = {};
    tv.tv_sec = 2;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in mcast_dst = {};
    mcast_dst.sin_family = AF_INET;
    mcast_dst.sin_port = htons(5353);
    mcast_dst.sin_addr.s_addr = inet_addr("224.0.0.251");

    uint32_t joined_ip = 0;
    int64_t last_announce_us = 0;

    auto build_mdns_a_packet = [](uint8_t* resp, uint16_t txid, const in_addr& ip_bin) -> size_t {
        std::memset(resp, 0, 64);
        resp[0] = static_cast<uint8_t>((txid >> 8) & 0xFF);
        resp[1] = static_cast<uint8_t>(txid & 0xFF);
        resp[2] = 0x84;  // QR=1, AA=1
        resp[3] = 0x00;
        resp[6] = 0x00;
        resp[7] = 0x01;  // ANCOUNT = 1

        size_t off = 12;
        const uint8_t qname[] = {4, 'r', 'o', 'b', 'i', 5, 'l', 'o', 'c', 'a', 'l', 0};
        std::memcpy(resp + off, qname, sizeof(qname));
        off += sizeof(qname);
        resp[off++] = 0x00;
        resp[off++] = 0x01;  // TYPE A
        resp[off++] = 0x80;  // CLASS IN + cache-flush
        resp[off++] = 0x01;
        resp[off++] = 0x00;
        resp[off++] = 0x00;
        resp[off++] = 0x00;
        resp[off++] = 120;   // TTL 120s
        resp[off++] = 0x00;
        resp[off++] = 0x04;  // RDLENGTH 4
        std::memcpy(resp + off, &ip_bin.s_addr, 4);
        off += 4;
        return off;
    };

    uint8_t buf[256];
    while (mdns_running_.load()) {
        std::string ip_str = WifiManager::GetInstance().GetIpAddress();
        in_addr ip_bin = {};
        bool have_ip = (!ip_str.empty() && inet_aton(ip_str.c_str(), &ip_bin) != 0 && ip_bin.s_addr != 0);

        if (have_ip && ip_bin.s_addr != joined_ip) {
            joined_ip = ip_bin.s_addr;
            ip_mreq mreq_if = {};
            mreq_if.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
            mreq_if.imr_interface.s_addr = ip_bin.s_addr;
            setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq_if, sizeof(mreq_if));
            setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq_if, sizeof(mreq_if));
            setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &ip_bin, sizeof(ip_bin));

            // Broadcast immediate gratuitous mDNS announcement when IP is acquired
            uint8_t ann[64];
            size_t alen = build_mdns_a_packet(ann, 0, ip_bin);
            sendto(sock, ann, alen, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
            last_announce_us = esp_timer_get_time();
        }

        int64_t now_us = esp_timer_get_time();
        if (have_ip && (now_us - last_announce_us) > 12LL * 1000000LL) {
            uint8_t ann[64];
            size_t alen = build_mdns_a_packet(ann, 0, ip_bin);
            sendto(sock, ann, alen, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
            last_announce_us = now_us;
        }

        sockaddr_in src = {};
        socklen_t slen = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &slen);
        if (n <= 0 || !have_ip) continue;

        uint16_t qtype = 1;
        if (!ParseMdnsRobiLocalQuery(buf, static_cast<size_t>(n), qtype)) {
            continue;
        }

        uint16_t txid = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
        uint8_t resp[64];
        size_t off = build_mdns_a_packet(resp, txid, ip_bin);

        sendto(sock, resp, off, 0, reinterpret_cast<sockaddr*>(&mcast_dst), sizeof(mcast_dst));
        sendto(sock, resp, off, 0, reinterpret_cast<sockaddr*>(&src), slen);
    }

    if (mdns_sock_ >= 0) {
        close(mdns_sock_);
        mdns_sock_ = -1;
    }
}
