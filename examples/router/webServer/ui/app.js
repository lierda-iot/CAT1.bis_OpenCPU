// Device Manager SPA — vanilla JS, no deps
const $ = s => document.querySelector(s);

function toast(t){const m=$('#msg');m.textContent=t;m.classList.add('show');setTimeout(()=>m.classList.remove('show'),2200);}
// Top center banner: ok=true green, false red
let _bannerTimer=null;
function banner(text, ok){
  const b=$('#banner');
  b.textContent=text;
  b.className='show '+(ok?'ok':'err');
  clearTimeout(_bannerTimer);
  _bannerTimer=setTimeout(()=>{b.className=b.className.replace('show','').trim();}, 2600);
}
function flash(d, okText){ banner(d&&d.ok ? (d.msg||okText||'Success') : (d&&d.msg||'Failed'), !!(d&&d.ok)); }

// GET result short cache (2.5s); POST never cached and clears cache.
const _cache = new Map();
const CACHE_TTL = 2500;
async function api(path, body, o){
  o = o || {};
  if(!body){
    const c = _cache.get(path);
    if(c && Date.now()-c.t < CACHE_TTL) return c.v;
  }
  const opt = {method: body?'POST':'GET'};
  if(body){opt.headers={'Content-Type':'application/x-www-form-urlencoded'};opt.body=body;}
  const r = await fetch(path, opt);
  const j = await r.json().catch(()=>({}));
  if(r.status===401 && !o.noBounce){ showLogin(); throw new Error('unauth'); }
  if(!r.ok){ const e=new Error(j.msg||'Request failed'); e.status=r.status; e.data=j; throw e; }
  if(!body) _cache.set(path, {t:Date.now(), v:j});
  else _cache.clear();
  return j;
}
function enc(o){return Object.entries(o).map(([k,v])=>k+'='+encodeURIComponent(v)).join('&');}

function showLogin(){$('#login').classList.remove('hide');$('#app').classList.add('hide');}
function showApp(){$('#login').classList.add('hide');$('#app').classList.remove('hide');render();}

async function doLogin(){
  try{
    const d = await api('/api/login', enc({user:$('#lu').value, pass:$('#lp').value}), {noBounce:true});
    if(d.ok){ if(d.mustChangePwd) banner('Please change the default password', true); showApp(); }
    else banner(d.msg||'Wrong username or password', false);
  }catch(e){
    banner(e.status===401 ? 'Wrong username or password' : (e.message||'Login failed'), false);
  }
}

// ── page definitions ──
const TABS = [
  {id:'status', name:'Status',       render:vStatus},
  {id:'network',name:'Network',      render:vNetwork},
  {id:'lan',    name:'LAN',          render:vLan},
  {id:'nat',    name:'Port Mapping', render:vNat},
  {id:'system', name:'System',       render:vSystem},
];
let cur = 'status';
let _navBuilt = false;
function buildNav(){
  const nav=$('#nav');nav.innerHTML='';
  TABS.forEach(x=>{
    const b=document.createElement('button');b.textContent=x.name;
    b.dataset.id=x.id;b.onclick=()=>switchTab(x.id);
    nav.appendChild(b);
  });
  _navBuilt=true;
}
function markActive(){
  $('#nav').querySelectorAll('button').forEach(b=>{
    b.className = b.dataset.id===cur ? 'active' : '';
  });
}
function switchTab(id){ if(id===cur)return; cur=id; render(); }
function render(){
  if(!_navBuilt) buildNav();
  markActive();
  const tab=TABS.find(x=>x.id===cur);
  if(!$('#view').innerHTML.trim()) $('#view').innerHTML='Loading...';
  tab.render();
}

function vStatus(){
  $('#view').innerHTML=`
  <div class="card"><h3>Device Info</h3><div id="c-dev"><div class="row"><span class="k">Loading…</span></div></div></div>
  <div class="card"><h3>WAN</h3><div id="c-wan"><div class="row"><span class="k">Loading…</span></div></div></div>`;

  api('/api/sysinfo').then(s=>{
    const el=$('#c-dev'); if(!el)return;
    el.innerHTML=`
    <div class="row"><span class="k">Model</span><span class="v">${s.model||'-'}</span></div>
    <div class="row"><span class="k">IMEI</span><span class="v">${s.imei||'-'}</span></div>
    <div class="row"><span class="k">SN</span><span class="v">${s.sn||'-'}</span></div>
    <div class="row"><span class="k">Firmware</span><span class="v">${s.fwVer||'-'}</span></div>
    <div class="row"><span class="k">Uptime</span><span class="v">${Math.floor(s.uptimeSec/3600)}h ${Math.floor(s.uptimeSec%3600/60)}m</span></div>`;
  }).catch(()=>{});

  api('/api/net/status').then(n=>{
    const el=$('#c-wan'); if(!el)return;
    el.innerHTML=`
    <div class="row"><span class="k">Status</span><span class="v">${n.wan.valid?'Online':'Offline'}</span></div>
    ${n.wan.valid?`<div class="row"><span class="k">IP</span><span class="v">${n.wan.ip}</span></div>
    <div class="row"><span class="k">Gateway</span><span class="v">${n.wan.gw}</span></div>
    <div class="row"><span class="k">Primary DNS</span><span class="v">${dnsShow(n.wan.dns1)}</span></div>
    <div class="row"><span class="k">Secondary DNS</span><span class="v">${dnsShow(n.wan.dns2)}</span></div>`:''}
    <div class="row"><span class="k">Signal</span><span class="v">${n.wan.rssi?n.wan.rssi+' dBm':'Unknown'}</span></div>`;
  }).catch(()=>{});
}

function dnsShow(v){ return (!v||v==='0.0.0.0') ? 'Auto (carrier)' : v; }
async function vNetwork(){
  const [n,c]=await Promise.all([api('/api/net/status'),api('/api/net/clients')]);
  let rows=c.clients.map(x=>`<div class="row"><span class="k">LAN${x.port} ${x.mac}</span><span class="v">${x.ip}</span></div>`).join('');
  if(!rows)rows='<div class="row"><span class="k">No clients</span></div>';
  $('#view').innerHTML=`
  <div class="card"><h3>LAN Link</h3>
    <div class="row"><span class="k">Gateway</span><span class="v">${n.lan.gw}</span></div>
    <div class="row"><span class="k">Netmask</span><span class="v">${n.lan.mask}</span></div>
    <div class="row"><span class="k">Clients</span><span class="v">${n.lan.clients}</span></div>
  </div>
  <div class="card"><h3>Connected Clients</h3>${rows}</div>`;
}

async function vLan(){
  const d=await api('/api/lan/config');
  $('#view').innerHTML=`
  <div class="card"><h3>Gateway / Netmask</h3>
    <label>Gateway IP</label><input id="gw" value="${d.gateway}">
    <label>Subnet Mask</label><input id="mask" value="${d.mask}">
    <button class="act" onclick="saveGw()">Save</button>
  </div>
  <div class="card"><h3>DHCP Pool</h3>
    <label>Start IP</label><input id="ps" value="${d.poolStart}">
    <label>End IP</label><input id="pe" value="${d.poolEnd}">
    <button class="act" onclick="saveDhcp()">Save</button>
  </div>
  <div class="card"><h3>DNS Servers</h3>
    <label>Primary DNS (blank = carrier)</label><input id="dns1" value="${d.dns1==='0.0.0.0'?'':d.dns1}" placeholder="8.8.8.8">
    <label>Secondary DNS (optional)</label><input id="dns2" value="${d.dns2==='0.0.0.0'?'':d.dns2}" placeholder="114.114.114.114">
    <button class="act" onclick="saveDns()">Save</button>
  </div>
  <div class="card"><h3>Static IP Binding</h3>
    <label>MAC</label><input id="bm" placeholder="aa:bb:cc:dd:ee:ff">
    <label>IP</label><input id="bi" placeholder="192.168.1.50">
    <button class="act" onclick="addBind()">Add</button>
  </div>`;
}

async function vNat(){
  const d=await api('/api/lan/config');
  $('#view').innerHTML=`
  <div class="card"><h3>Port Mapping Rules</h3>
    ${natRows(d.nat)}
  </div>
  <div class="card"><h3>Add Rule</h3>
    <label>Protocol</label>
    <select id="np"><option value="6">TCP</option><option value="17">UDP</option></select>
    <label>Ext Port</label><input id="nep" type="number" min="1" max="65535" placeholder="8080">
    <label>Internal IP</label><input id="nip" placeholder="192.168.1.50">
    <label>Internal Port</label><input id="nipp" type="number" min="1" max="65535" placeholder="80">
    <button class="act" onclick="addNat()">Add</button>
  </div>`;
}
function natRows(list){
  if(!list||!list.length) return '<div class="row"><span class="k">No rules</span></div>';
  return list.map(r=>`<div class="row">
    <span class="k">${r.proto==17?'UDP':'TCP'} ext${r.extPort} → ${r.intIp}:${r.intPort}</span>
    <span class="v"><a href="#" onclick="delNat(${r.extPort});return false" style="color:#e5484d">Delete</a></span>
  </div>`).join('');
}
async function saveGw(){try{const d=await api('/api/lan/gateway',enc({gw:$('#gw').value,mask:$('#mask').value}));flash(d,'Saved');}catch(e){banner(e.message||'Save failed',false);}}
async function saveDhcp(){try{const d=await api('/api/lan/dhcp',enc({start:$('#ps').value,end:$('#pe').value}));flash(d,'Saved');}catch(e){banner(e.message||'Save failed',false);}}
async function saveDns(){try{const d=await api('/api/lan/dns',enc({dns1:$('#dns1').value.trim(),dns2:$('#dns2').value.trim()}));flash(d,'Saved');}catch(e){banner(e.message||'Save failed',false);}}
async function addBind(){try{const d=await api('/api/lan/bind',enc({action:'add',mac:$('#bm').value,ip:$('#bi').value}));flash(d,'Added');}catch(e){banner(e.message||'Add failed',false);}}
async function addNat(){try{const d=await api('/api/lan/nat',enc({action:'add',proto:$('#np').value,extPort:$('#nep').value,intIp:$('#nip').value,intPort:$('#nipp').value}));flash(d,'Added'); if(d.ok){_cache.clear();vNat();}}catch(e){banner(e.message||'Add failed',false);}}
async function delNat(ext){try{const d=await api('/api/lan/nat',enc({action:'del',extPort:ext}));flash(d,'Deleted'); if(d.ok){_cache.clear();vNat();}}catch(e){banner(e.message||'Delete failed',false);}}

function vSystem(){
  $('#view').innerHTML=`
  <div class="card"><h3>Change Password</h3>
    <label>Old Password</label><input id="op" type="password">
    <label>New Password (≥6)</label><input id="np" type="password">
    <button class="act" onclick="chpwd()">Change</button>
  </div>
  <div class="card"><h3>Firmware Upgrade</h3>
    <input type="file" id="fw" style="display:none" onchange="document.getElementById('fwBox').value=this.files[0]?this.files[0].name:''">
    <div style="display:flex;gap:8px;align-items:center">
      <button class="act" style="margin-top:0;flex:none" onclick="document.getElementById('fw').click()">Choose File</button>
      <input id="fwBox" readonly placeholder="No file selected" style="flex:1">
    </div>
    <button class="act" onclick="upload()">Upload &amp; Upgrade</button>
  </div>
  <div class="card"><h3>Maintenance</h3>
    <button class="act" onclick="reboot()">Reboot</button>
    <button class="act danger" onclick="factory()">Factory Reset</button>
  </div>`;
}
async function chpwd(){
  try{const d=await api('/api/passwd',enc({old:$('#op').value,new:$('#np').value}));
    flash(d,'Changed'); if(d.ok)setTimeout(showLogin,1200);
  }catch(e){banner(e.message||'Change failed',false);}
}
async function reboot(){
  if(!confirm('Confirm reboot?'))return;
  try{ await api('/api/system/reboot','1'); }catch(e){}
  banner('Rebooting, please log in again later', true);
  _cache.clear();
  setTimeout(showLogin, 1000);
}
async function factory(){
  if(!confirm('Factory reset will clear all config. Confirm?'))return;
  try{
    const d=await api('/api/system/factory','1');
    if(d.ok){ banner(d.msg||'Factory reset done, rebooting', true); _cache.clear(); setTimeout(showLogin,1000); }
    else banner(d.msg||'Factory reset failed', false);
  }catch(e){ banner(e.message||'Factory reset failed', false); }
}
async function upload(){
  const f=$('#fw').files[0]; if(!f){banner('Please select a file',false);return;}
  banner('Uploading...',true);
  try{
    const r=await fetch('/api/system/upgrade',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f});
    const d=await r.json().catch(()=>({}));
    if(d.ok){ banner(d.msg||'Verified, rebooting',true); _cache.clear(); setTimeout(showLogin,1200); }
    else banner(d.msg||'Upgrade failed',false);
  }catch(e){ banner('Upgrade failed',false); }
}

// Startup: show login page, then probe session in background
showLogin();
(async()=>{
  try{ const r=await fetch('/api/sysinfo'); if(r.status===200) showApp(); }
  catch(e){}
})();
