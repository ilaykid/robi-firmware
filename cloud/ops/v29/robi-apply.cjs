#!/usr/bin/env node
/* Robi's answers into a child's state, by the app's own code (docs/operations.md §9).
   node scripts/robi-apply.cjs [--site <dir>] < in.json > out.json

   --site: a folder with index.html, data/bank.json and data/upper.json, the files of the live site (default: public/ of
   this repository). The app script is taken from index.html and run in Node as the tests do (scripts/test-*.cjs); no
   network, no packages. Use the files of the version that is live, so the state is written the way the site writes it.

   stdin  {"kid": <one row of robi_kids: id, name, grade, focus, test_date, state, updated_at>,
           "answers": [{"t": <ms since 1970 or ISO time>, "q": "<question id>", "answer": <option index 0-3, or the
                        number for a typed question>, "ok": true|false, "ms": <time on the question>}, ...],
           "now": <optional, ms; for tests>}
          Other fields of an answer (kid name, type, lvl) are ignored: type and level come from the bank, as in the app.
   stdout {"kid_id", "version": <updated_at of the row read, to pass to robi_save>, "changed": <true when any answer
           was applied>, "state": <the new state, for robi_save>, "applied": [<question ids>],
           "skipped": [{"q", "t", "reason"}], "levels": {<type>: {"lvl": 1-3, "n": answers, "mastery": 0-100|null}}}
   With "answers": [] it changes nothing and gives the levels (the level the site would pick for each type of the
   child's track: levelFor).

   Each answer goes through recordAnswer, the function the app calls when a child answers a practice question, with the
   source 'robi' (it then counts as practice: rating per type, the mistake notebook, the answer log, `seen`, streak and
   days). recordAnswer runs at the time of the answer (Date.now is the answer's t). Nothing is sent anywhere: in the app
   recordAnswer also queues the answer for the answers database, but only for a signed-in account, and there is none
   here, so Robi's answers stay out of `attempts`, D1 and difficulty calibration.
   An answer is skipped (and listed with the reason) when its question is unknown, belongs to the other track (grades
   6-9 vs. 2-3), is a drag question, its time is in the future or more than 30 days old, `ok` disagrees with the key
   for the answer given, or it was already applied (each answer gets an event id made from the child, the time and the
   question, so running the same answers again after a lost reply adds nothing twice). Exit 2 on bad input. */
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),crypto=require('node:crypto');

function load(site){
  const html=fs.readFileSync(path.join(site,'index.html'),'utf8');
  const app=[...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]).find(s=>s.includes('function recordAnswer('));
  if(!app)throw new Error('no app script in '+path.join(site,'index.html'));
  const js=app.split("if(typeof document!=='undefined'){")[0];
  const err=(...a)=>process.stderr.write(a.map(String).join(' ')+'\n');
  const ctx=vm.createContext({console:{log:err,warn:err,error:err,info:err},setTimeout,clearTimeout,TextEncoder});
  vm.runInContext(js,ctx);
  ctx.__BANK=JSON.parse(fs.readFileSync(path.join(site,'data/bank.json'),'utf8'));vm.runInContext('BANK=indexBank(__BANK);delete globalThis.__BANK',ctx); /* data, not code */
  const up=path.join(site,'data/upper.json');
  if(fs.existsSync(up)){ctx.__UP=JSON.parse(fs.readFileSync(up,'utf8'));vm.runInContext('enableUpper(__UP);delete globalThis.__UP',ctx)}
  /* the app's clock is the answer's time while it is recorded, and `now` otherwise */
  vm.runInContext('(()=>{const real=Date.now;Date.now=()=>typeof __T==="number"?__T:typeof __NOW==="number"?__NOW:real()})()',ctx);
  return ctx;
}

const DAY=86400000;
function apply(ctx,input){
  const {kid:row,answers=[],now=Date.now()}=input||{};
  if(!row||typeof row.id!=='string'||!row.state||typeof row.state!=='object'||Array.isArray(row.state))throw new Error('kid: a row of robi_kids is needed (id, name, grade, focus, test_date, state, updated_at)');
  if(!Array.isArray(answers))throw new Error('answers: an array is needed');
  ctx.__NOW=now;ctx.__T=undefined;
  ctx.__ROW=row;const kid=vm.runInContext('rowToKid(__ROW)',ctx);delete ctx.__ROW;
  const {BANK,isRight,recordAnswer,kidToRow,focusTypes,levelFor,mastery,evOrder}=vm.runInContext('({BANK,isRight,recordAnswer,kidToRow,focusTypes,levelFor,mastery,evOrder})',ctx);
  const isU=q=>q.st==='U'||/^u/.test(q.type);
  const applied=[],skipped=[];
  const list=answers.map(a=>({...a,t:typeof (a&&a.t)==='string'?Date.parse(a.t):a&&a.t})).sort((x,y)=>(x.t||0)-(y.t||0));
  for(const a of list){
    const skip=reason=>skipped.push({q:a.q,t:a.t,reason});
    if(!Number.isFinite(a.t))skip('bad time');
    else if(a.t>now+5*60000)skip('time in the future');
    else if(a.t<now-30*DAY)skip('older than 30 days');
    else{
      const q=typeof a.q==='string'&&BANK.byId[a.q];
      if(!q){skip('unknown question');continue}
      if(isU(q)!==(kid.focus==='U')){skip("not in this child's track");continue}
      if(q.place){skip('drag question');continue}
      /* the answer as the app keeps it: the option index, or the typed digits */
      let ans=null;const v=a.answer;
      if(q.input){if(v!==null&&v!==undefined&&/^\d{1,9}$/.test(String(v).trim()))ans=String(Number(String(v).trim()))}
      else{const n=typeof v==='number'?v:typeof v==='string'&&/^\d{1,2}$/.test(v.trim())?Number(v):NaN;if(Number.isInteger(n)&&n>=0&&n<q.options.length)ans=n}
      let ok;
      if(ans!==null){ok=!!isRight(q,ans);if(typeof a.ok==='boolean'&&a.ok!==ok){skip('ok does not match the key');continue}}
      else if(typeof a.ok==='boolean')ok=a.ok;
      else{skip('no answer and no ok');continue}
      const ms=Number.isFinite(a.ms)&&a.ms>0&&a.ms<=1800000?Math.round(a.ms):null;
      const id='r'+crypto.createHash('sha256').update(kid.id+'|'+a.t+'|'+q.id).digest('hex').slice(0,7);
      if(kid.log.some(l=>l.t===a.t&&l.i===id)){skip('already applied');continue}
      ctx.__T=a.t;
      try{recordAnswer(kid,q,ans,ok,ms,'robi')}finally{ctx.__T=undefined}
      /* the event id of this answer (recordAnswer gave it one of the page's own): the same answer twice is found */
      const l=kid.log[kid.log.length-1];if(l&&l.t===a.t)l.i=id;
      const r=kid.recent[kid.recent.length-1];if(r&&r.t===a.t&&r.q&&r.q.id===q.id)r.i=id;
      applied.push(q.id);
    }
  }
  /* an answer older than the last one on the site goes to its place in time, as mergeKid orders them */
  if(applied.length){kid.log.sort(evOrder);kid.recent.sort(evOrder)}
  const levels={};
  for(const t of focusTypes(kid)){const s=kid.skills[t];levels[t]={lvl:levelFor(s),n:s?s.n:0,mastery:mastery(s)}}
  /* robi_save accepts a change only in what an answer changes (ops.robi_state_problem): everything else goes back
     exactly as it was read, even if the app's loading code would tidy it */
  const full=JSON.parse(JSON.stringify(kidToRow(kid).state)),state={...row.state};
  for(const k of ['log','recent','mistakes','skills','seen'])if(full[k]!==undefined)state[k]=full[k];
  /* what waits for the questions of the grades 6-9 track (no data/upper.json here) sits in `away`, outside the lists
     Robi may change: saving now would drop it from them, so nothing is saved until the file is here */
  if(full.away){applied.forEach(q=>skipped.push({q,reason:'the grades 6-9 questions are not loaded here'}));applied.length=0;
    return {kid_id:row.id,version:row.updated_at,changed:false,state:row.state,applied,skipped,levels}}
  return {kid_id:row.id,version:row.updated_at,changed:applied.length>0,state,applied,skipped,levels};
}

module.exports={load,apply};
if(require.main===module){
  const args=process.argv.slice(2),i=args.indexOf('--site');
  const site=i>=0?args[i+1]:path.resolve(__dirname,'../public');
  let input;
  try{input=JSON.parse(fs.readFileSync(0,'utf8'))}catch(e){process.stderr.write('stdin is not JSON: '+e.message+'\n');process.exit(2)}
  try{process.stdout.write(JSON.stringify(apply(load(site),input))+'\n')}
  catch(e){process.stderr.write(String(e&&e.message||e)+'\n');process.exit(2)}
}
