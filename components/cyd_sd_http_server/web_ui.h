#pragma once

namespace esphome {
namespace cyd_sd_http_server {

static const char WEB_UI[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD SD-Dateiserver</title><style>
:root{color-scheme:light dark;font:16px system-ui,sans-serif}body{max-width:960px;margin:2rem auto;padding:0 1rem}
fieldset{border:1px solid #888;border-radius:.5rem;margin:1rem 0}input,button{font:inherit;padding:.45rem;margin:.2rem}
button:disabled{opacity:.5}table{width:100%;border-collapse:collapse}th,td{padding:.5rem;border-bottom:1px solid #777;text-align:left}
#message{white-space:pre-wrap;padding:.7rem;border-radius:.4rem}.ok{background:#174d2c;color:#fff}.error{background:#792525;color:#fff}
progress{width:100%}.muted{opacity:.75}.actions{white-space:nowrap}
</style></head><body><h1>CYD SD-Dateiserver</h1>
<fieldset><legend>Zugang</legend><label>API-Schlüssel <input id="key" type="password" autocomplete="off"></label>
<button id="connect">Verbinden</button><span class="muted">Nur für ein vertrauenswürdiges LAN.</span></fieldset>
<div id="message" hidden></div><p id="health">Nicht verbunden</p>
<fieldset><legend>Dateien</legend><p><button id="up">↑ Übergeordnet</button><strong id="path">/</strong></p>
<p><input id="file" type="file"><button id="upload">Hochladen</button><label><input id="overwrite" type="checkbox"> überschreiben</label></p>
<progress id="progress" value="0" max="100"></progress><p><button id="mkdir">Verzeichnis anlegen</button><button id="refresh">Aktualisieren</button></p>
<table><thead><tr><th>Name</th><th>Typ</th><th>Größe</th><th>Aktionen</th></tr></thead><tbody id="entries"></tbody></table></fieldset>
<script>
'use strict';const $=id=>document.getElementById(id);let current='/';let busy=false;
const api=p=>p;function key(){return $('key').value}function headers(){return {'X-API-Key':key()}}
function show(text,ok=false){const m=$('message');m.hidden=false;m.className=ok?'ok':'error';m.textContent=text}
function lock(value){busy=value;document.querySelectorAll('button,input[type=file]').forEach(e=>e.disabled=value)}
async function jsonRequest(url,options={}){options.headers={...headers(),...(options.headers||{})};let response;
try{response=await fetch(url,options)}catch(e){throw new Error('Netzwerkfehler: '+e.message)}
let data;try{data=await response.json()}catch(e){throw new Error('Ungültige Serverantwort (HTTP '+response.status+')')}
if(!response.ok||!data.ok)throw new Error(data.message||('HTTP '+response.status));return data}
function childPath(name){return current==='/'?'/'+name:current+'/'+name}
function formatBytes(n){if(n<1024)return n+' B';for(const u of ['KiB','MiB','GiB']){n/=1024;if(n<1024)return n.toFixed(1)+' '+u}return n.toFixed(1)+' TiB'}
async function refresh(){if(!key())return show('API-Schlüssel fehlt');lock(true);try{sessionStorage.setItem('cydSdApiKey',key());
const health=await jsonRequest(api('/sd/api/health'));$('health').textContent=health.mounted?`SD ${health.card_type}: ${formatBytes(health.free_bytes)} frei von ${formatBytes(health.total_bytes)}`:'SD nicht verfügbar';
const listing=await jsonRequest(api('/sd/api/list?path='+encodeURIComponent(current)+'&limit=100'));$('path').textContent=current;const body=$('entries');body.replaceChildren();
for(const entry of listing.entries){const tr=document.createElement('tr');const name=document.createElement('td');const open=document.createElement('button');open.textContent=entry.name;
open.onclick=()=>entry.type==='directory'?navigate(entry.path):download(entry.path,entry.name);name.append(open);const type=document.createElement('td');type.textContent=entry.type;
const size=document.createElement('td');size.textContent=entry.type==='file'?formatBytes(entry.size):'—';const actions=document.createElement('td');actions.className='actions';
if(entry.type==='file'){const dl=document.createElement('button');dl.textContent='Download';dl.onclick=()=>download(entry.path,entry.name);actions.append(dl)}
const del=document.createElement('button');del.textContent='Löschen';del.onclick=()=>removeEntry(entry);actions.append(del);tr.append(name,type,size,actions);body.append(tr)}
if(listing.has_more)show('Das Verzeichnis enthält weitere Einträge. Nutze die HTTP-API mit offset zur Pagination.');
}catch(e){show(e.message)}finally{lock(false)}}
function navigate(path){current=path;refresh()}function parent(){if(current==='/')return;current=current.slice(0,current.lastIndexOf('/'))||'/';refresh()}
async function upload(){const file=$('file').files[0];if(!file)return show('Bitte eine Datei auswählen');const target=childPath(file.name);lock(true);$('progress').value=0;
await new Promise(resolve=>{const xhr=new XMLHttpRequest();xhr.open('POST','/sd/api/file?path='+encodeURIComponent(target)+'&overwrite='+$('overwrite').checked);xhr.setRequestHeader('X-API-Key',key());xhr.setRequestHeader('Content-Type','application/octet-stream');
xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=e.loaded/e.total*100};xhr.onerror=()=>{show('Netzwerkfehler beim Upload');resolve()};xhr.onload=()=>{let d;try{d=JSON.parse(xhr.responseText)}catch(e){show('Ungültige Serverantwort (HTTP '+xhr.status+')');resolve();return}show(d.message||`${d.bytes} Bytes übertragen`,xhr.status>=200&&xhr.status<300&&d.ok);resolve()};xhr.send(file)});lock(false);await refresh()}
async function download(path,name){lock(true);try{const response=await fetch('/sd/api/file?path='+encodeURIComponent(path),{headers:headers()});if(!response.ok){let d;try{d=await response.json()}catch(e){}throw new Error(d?.message||('Download fehlgeschlagen: HTTP '+response.status))}
const blob=await response.blob();const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();URL.revokeObjectURL(url)}catch(e){show(e.message)}finally{lock(false)}}
async function removeEntry(entry){if(!confirm(`${entry.path} wirklich löschen?`))return;lock(true);try{const kind=entry.type==='directory'?'directory':'file';await jsonRequest('/sd/api/'+kind+'?path='+encodeURIComponent(entry.path),{method:'DELETE'});show('Gelöscht: '+entry.path,true)}catch(e){show(e.message)}finally{lock(false);await refresh()}}
async function mkdir(){const name=prompt('Name des neuen Verzeichnisses');if(!name)return;lock(true);try{const path=childPath(name);await jsonRequest('/sd/api/directory?path='+encodeURIComponent(path),{method:'POST'});show('Angelegt: '+path,true)}catch(e){show(e.message)}finally{lock(false);await refresh()}}
$('connect').onclick=refresh;$('refresh').onclick=refresh;$('up').onclick=parent;$('upload').onclick=upload;$('mkdir').onclick=mkdir;
$('key').value=sessionStorage.getItem('cydSdApiKey')||'';if($('key').value)refresh();
</script></body></html>)HTML";

}  // namespace cyd_sd_http_server
}  // namespace esphome

