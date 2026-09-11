#pragma once

namespace esphome {
namespace cyd_sd_http_server {

static const char WEB_UI[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>CYD SD-Dateiserver</title>
<script>
try{const theme=localStorage.getItem('cydSdTheme');if(theme==='light'||theme==='dark')document.documentElement.dataset.theme=theme}catch(e){}
</script><style>
:root{color-scheme:light;--bg:#f2f5fa;--surface:#fff;--inset:#eaf0f7;--text:#1d293b;--muted:#526179;--border:#8291a7;--accent:#3154b8;--on-accent:#fff;--danger:#a32638;--ok-bg:#e0f4e8;--ok-text:#175332;--error-bg:#fce8eb;--error-text:#8a2031;font:16px/1.5 system-ui,sans-serif}
@media(prefers-color-scheme:dark){
  :root:not([data-theme="light"]){color-scheme:dark;--bg:#10141e;--surface:#1a2130;--inset:#232d40;--text:#edf2fc;--muted:#b3c0d6;--border:#65748e;--accent:#aec1ff;--on-accent:#172342;--danger:#ffa8b4;--ok-bg:#183e30;--ok-text:#b0f0cb;--error-bg:#492631;--error-text:#ffc0c9}
}
:root[data-theme="dark"]{color-scheme:dark;--bg:#10141e;--surface:#1a2130;--inset:#232d40;--text:#edf2fc;--muted:#b3c0d6;--border:#65748e;--accent:#aec1ff;--on-accent:#172342;--danger:#ffa8b4;--ok-bg:#183e30;--ok-text:#b0f0cb;--error-bg:#492631;--error-text:#ffc0c9}
*{box-sizing:border-box}body{max-width:1040px;margin:2.5rem auto;padding:0 1.25rem;background:var(--bg);color:var(--text)}
header,.toolbar,.access-row{display:flex;align-items:center;flex-wrap:wrap;gap:.75rem}header{justify-content:space-between;margin-bottom:1.5rem}
h1{margin:0;font-size:clamp(1.4rem,4vw,2rem);letter-spacing:-.035em}header p{margin:.25rem 0 0}.preferences{display:flex;flex-wrap:wrap;gap:.75rem}.picker{display:flex;align-items:center;gap:.5rem}
fieldset{min-width:0;margin:1.25rem 0;padding:1.25rem;border:1px solid var(--border);border-radius:1rem;background:var(--surface)}legend{padding:0 .5rem;font-weight:700}
input,button,select{font:inherit;color:var(--text)}button,select,input[type=password],input[type=file]{min-height:44px;max-width:100%;border:1px solid var(--border);border-radius:.6rem;background:var(--inset)}
button,select,input[type=password]{padding:.6rem .85rem}button{cursor:pointer;font-weight:600;overflow-wrap:anywhere}button.primary{background:var(--accent);border-color:var(--accent);color:var(--on-accent)}
button.danger{color:var(--danger)}button:disabled,input:disabled{opacity:.55;cursor:wait}
@media(hover:hover){button:not(:disabled):hover{border-color:var(--accent);box-shadow:inset 0 0 0 1px var(--accent)}}
:where(button,input,select):focus-visible{outline:3px solid var(--accent);outline-offset:3px}
.key-field,.file-field{display:grid;gap:.4rem;flex:1 1 18rem;min-width:0}.access-row{align-items:flex-end}.key-field input{width:100%}
.muted,#health{color:var(--muted)}.hint{margin:.8rem 0 0;font-size:.9rem}#health{margin:1rem .25rem;overflow-wrap:anywhere}
#message{white-space:pre-wrap;overflow-wrap:anywhere;padding:1rem;border:1px solid;border-radius:.75rem}.ok{background:var(--ok-bg);color:var(--ok-text)}.error{background:var(--error-bg);color:var(--error-text)}
.toolbar{margin:0 0 1rem}.breadcrumb{padding-bottom:1rem;border-bottom:1px solid var(--border)}#path{min-width:0;flex:1 1 12rem;overflow-wrap:anywhere;font-family:ui-monospace,monospace}
.upload-row{align-items:flex-end}.check{display:flex;align-items:center;gap:.5rem;min-height:44px;cursor:pointer}.check input{width:1.2rem;height:1.2rem;margin:0;accent-color:var(--accent)}
.file-control{position:relative;display:flex;align-items:center;flex-wrap:wrap;gap:.5rem;min-width:0;padding:.3rem;border:1px solid var(--border);border-radius:.6rem;background:var(--inset);cursor:pointer}
.file-control input{position:absolute;width:1px;height:1px;min-height:0;padding:0;overflow:hidden;clip-path:inset(50%)}.file-control:focus-within{outline:3px solid var(--accent);outline-offset:3px}
.file-button{display:flex;align-items:center;min-height:44px;padding:.35rem .6rem;background:var(--surface);border:1px solid var(--border);border-radius:.35rem}.file-control:has(input:disabled){opacity:.55;cursor:wait}
#filename{flex:1 1 8rem;min-width:0;overflow-wrap:anywhere;font-size:.9rem}
progress{display:block;width:100%;height:.5rem;margin:.25rem 0 1.25rem;border:0;border-radius:1rem;overflow:hidden;background:var(--inset);accent-color:var(--accent)}
progress::-webkit-progress-bar{background:var(--inset)}progress::-webkit-progress-value{background:var(--accent)}progress::-moz-progress-bar{background:var(--accent)}
table{width:100%;border-collapse:collapse;table-layout:fixed}th,td{padding:.85rem .5rem;border-bottom:1px solid var(--border);text-align:left;overflow-wrap:anywhere}
th{color:var(--muted);font-size:.85rem;font-weight:600}th:first-child{width:40%}th:nth-child(2),th:nth-child(3){width:15%}th:last-child{width:30%}
.entry-name{max-width:100%;padding:.5rem;text-align:left;background:transparent;border-color:transparent;color:var(--accent)}.actions button{margin:.2rem .35rem .2rem 0;font-size:.9rem}
.empty{text-align:center;color:var(--muted);padding:2rem 1rem}
@media(max-width:640px){
  body{margin:1rem auto;padding:0 .75rem}header{gap:1rem;margin-bottom:1rem}fieldset{padding:.85rem;border-radius:.75rem}
  .access-row>button,.upload-row>button{flex:1 1 auto}.key-field,.file-field{flex-basis:100%}.toolbar>button{flex-grow:1}
  table,tbody{display:block}thead{position:absolute;width:1px;height:1px;padding:0;overflow:hidden;clip-path:inset(50%);white-space:nowrap}
  tbody tr{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:.35rem .75rem;padding:.85rem 0;border-bottom:1px solid var(--border)}
  td{min-width:0;padding:0;border:0}td:first-child,td.actions,td.empty{grid-column:1/-1}.entry-name{width:100%;padding:.5rem 0}
  td[data-label]{color:var(--muted);font-size:.9rem}td[data-label]::before{content:attr(data-label) ': ';font-weight:600}
  .actions{display:flex;flex-wrap:wrap;gap:.5rem;margin-top:.5rem}.actions button{flex:1 1 auto;margin:0}.empty{padding:1rem}
}
</style></head><body><header><div><h1 data-i18n="title">CYD SD-Dateiserver</h1><p class="muted" data-i18n="subtitle">Dateien auf deiner SD-Karte verwalten</p></div>
<div class="preferences"><label class="picker" for="language"><span data-i18n="language">Sprache</span><select id="language"><option value="de" lang="de">Deutsch</option><option value="en" lang="en">English</option></select></label>
<label class="picker" for="theme"><span data-i18n="theme">Design</span><select id="theme"><option value="auto" data-i18n="auto">Automatisch</option><option value="light" data-i18n="light">Hell</option><option value="dark" data-i18n="dark">Dunkel</option></select></label></div></header>
<main><fieldset><legend data-i18n="access">Zugang</legend><div class="access-row"><label class="key-field" for="key"><span data-i18n="apiKey">API-Schlüssel</span><input id="key" type="password" autocomplete="off"></label>
<button id="connect" class="primary" data-i18n="connect">Verbinden</button></div><p class="muted hint" data-i18n="trustedLan">Nur für ein vertrauenswürdiges LAN.</p></fieldset>
<div id="message" role="status" aria-live="polite" hidden></div><p id="health" role="status" data-i18n="disconnected">Nicht verbunden</p>
<fieldset><legend data-i18n="files">Dateien</legend><div class="toolbar breadcrumb"><button id="up" data-i18n="parent">↑ Übergeordnet</button><strong id="path" data-i18n-aria="currentDirectory" aria-label="Aktuelles Verzeichnis">/</strong></div>
<div class="toolbar upload-row"><div class="file-field"><span id="file-label" data-i18n="chooseFile">Datei auswählen</span><label class="file-control" for="file"><input id="file" type="file" aria-labelledby="file-label" aria-describedby="filename"><span class="file-button" data-i18n="browse">Durchsuchen…</span><span id="filename" class="muted" data-i18n="noFile">Keine Datei ausgewählt</span></label></div>
<button id="upload" class="primary" data-i18n="upload">Hochladen</button><label class="check"><input id="overwrite" type="checkbox"><span data-i18n="overwrite">Überschreiben</span></label></div>
<progress id="progress" value="0" max="100" data-i18n-aria="uploadProgress" aria-label="Upload-Fortschritt"></progress><div class="toolbar"><button id="mkdir" data-i18n="mkdir">Verzeichnis anlegen</button><button id="refresh" data-i18n="refresh">Aktualisieren</button></div>
<table role="table" data-i18n-aria="directoryFiles" aria-label="Dateien im aktuellen Verzeichnis"><thead role="rowgroup"><tr role="row"><th scope="col" role="columnheader" data-i18n="name">Name</th><th scope="col" role="columnheader" data-i18n="type">Typ</th><th scope="col" role="columnheader" data-i18n="size">Größe</th><th scope="col" role="columnheader" data-i18n="actions">Aktionen</th></tr></thead><tbody id="entries" role="rowgroup"></tbody></table></fieldset></main>
<script>
'use strict';const $=id=>document.getElementById(id);let current='/';let busy=false;
// Each entry contains German and English. Keep filenames and server paths as plain text.
const translations={
  title:['CYD SD-Dateiserver','CYD SD File Server'],subtitle:['Dateien auf deiner SD-Karte verwalten','Manage files on your SD card'],
  language:['Sprache','Language'],theme:['Design','Theme'],auto:['Automatisch','Automatic'],light:['Hell','Light'],dark:['Dunkel','Dark'],
  access:['Zugang','Access'],apiKey:['API-Schlüssel','API key'],connect:['Verbinden','Connect'],trustedLan:['Nur für ein vertrauenswürdiges LAN.','For use on a trusted LAN only.'],
  disconnected:['Nicht verbunden','Not connected'],files:['Dateien','Files'],parent:['↑ Übergeordnet','↑ Parent folder'],currentDirectory:['Aktuelles Verzeichnis','Current directory'],
  chooseFile:['Datei auswählen','Select a file'],browse:['Durchsuchen…','Browse…'],noFile:['Keine Datei ausgewählt','No file selected'],upload:['Hochladen','Upload'],overwrite:['Überschreiben','Overwrite'],
  uploadProgress:['Upload-Fortschritt','Upload progress'],mkdir:['Verzeichnis anlegen','Create directory'],refresh:['Aktualisieren','Refresh'],directoryFiles:['Dateien im aktuellen Verzeichnis','Files in the current directory'],
  name:['Name','Name'],type:['Typ','Type'],size:['Größe','Size'],actions:['Aktionen','Actions'],directory:['Ordner','Folder'],file:['Datei','File'],download:['Herunterladen','Download'],remove:['Löschen','Delete'],
  empty:['Dieses Verzeichnis ist leer.','This directory is empty.'],missingKey:['API-Schlüssel fehlt','API key is missing'],
  health:['SD {card}: {free} frei von {total}','SD {card}: {free} free of {total}'],unavailable:['SD nicht verfügbar','SD unavailable'],
  more:['Das Verzeichnis enthält weitere Einträge. Nutze die HTTP-API mit offset zur Pagination.','This directory has more entries. Use the HTTP API with offset for pagination.'],
  network:['Netzwerkfehler: {detail}','Network error: {detail}'],invalidResponse:['Ungültige Serverantwort (HTTP {status})','Invalid server response (HTTP {status})'],httpError:['Anfrage fehlgeschlagen (HTTP {status})','Request failed (HTTP {status})'],
  selectFile:['Bitte eine Datei auswählen','Please select a file'],uploadNetwork:['Netzwerkfehler beim Upload','Network error during upload'],uploaded:['{bytes} Bytes übertragen','{bytes} bytes uploaded'],
  confirmDelete:['{path} wirklich löschen?','Really delete {path}?'],deleted:['Gelöscht: {path}','Deleted: {path}'],directoryName:['Name des neuen Verzeichnisses','Name of the new directory'],created:['Angelegt: {path}','Created: {path}'],
  UNAUTHORIZED:['API-Schlüssel fehlt oder ist ungültig','API key is missing or invalid'],NOT_MOUNTED:['SD-Karte ist nicht eingebunden','SD card is not mounted'],INVALID_PATH:['Der Pfad fehlt oder ist ungültig','Path is missing or invalid'],
  NOT_FOUND:['Pfad wurde nicht gefunden','Path was not found'],ALREADY_EXISTS:['Das Ziel existiert bereits','Target already exists'],NOT_A_FILE:['Pfad ist keine reguläre Datei','Path is not a regular file'],NOT_A_DIRECTORY:['Pfad ist kein Verzeichnis','Path is not a directory'],
  OPEN_FAILED:['Datei konnte nicht geöffnet werden','File could not be opened'],READ_FAILED:['Lesen von der SD-Karte fehlgeschlagen','SD card read failed'],SEEK_FAILED:['Positionieren in der Datei fehlgeschlagen','SD card seek failed'],
  WRITE_FAILED:['Schreiben auf die SD-Karte fehlgeschlagen','SD card write failed'],SHORT_WRITE:['Die SD-Karte hat weniger Bytes als angefordert gespeichert','SD card accepted fewer bytes than requested'],
  FLUSH_FAILED:['Schreibpuffer konnte nicht gespeichert werden','File flush failed'],CLOSE_FAILED:['Datei konnte nicht geschlossen werden','File close failed'],RENAME_FAILED:['Abschließendes Umbenennen fehlgeschlagen','Atomic rename step failed'],
  DELETE_FAILED:['Löschen fehlgeschlagen','Delete failed'],CREATE_DIRECTORY_FAILED:['Verzeichnis konnte nicht angelegt werden','Directory creation failed'],NO_SPACE:['Nicht genügend freier Speicher','Not enough free space'],
  BUSY:['Die SD-Karte wird gerade gelesen oder beschrieben','SD card operation conflicts with an active reader or upload'],SIZE_MISMATCH:['Gespeicherte Größe stimmt nicht mit Content-Length überein','Stored size differs from Content-Length'],
  LIST_LIMIT_REACHED:['Maximale Anzahl der Verzeichniseinträge erreicht','Directory result limit reached'],UNKNOWN:['Unbekannter Speicherfehler','Unknown storage error'],
  LENGTH_REQUIRED:['Content-Length fehlt','Content-Length is required'],INVALID_LENGTH:['Content-Length ist ungültig oder widersprüchlich','Content-Length is invalid or inconsistent'],UPLOAD_TOO_LARGE:['Die Datei überschreitet die maximale Upload-Größe','File exceeds the maximum upload size'],
  INVALID_CONTENT_TYPE:['Content-Type muss application/octet-stream sein','Content-Type must be application/octet-stream'],INVALID_OVERWRITE:['overwrite muss true oder false sein','overwrite must be true or false'],INVALID_OFFSET:['offset ist ungültig','offset is invalid'],INVALID_LIMIT:['limit muss zwischen 1 und 100 liegen','limit must be between 1 and 100']
};
let language='de';try{const saved=localStorage.getItem('cydSdLanguage');if(saved==='de'||saved==='en')language=saved}catch(e){}
function t(id,args={}){return translations[id][language==='en'?1:0].replace(/\{(\w+)\}/g,(_,key)=>args[key]??'')}
function translated(element,id,args={}){element.dataset.i18n=id;element.dataset.i18nArgs=JSON.stringify(args);element.textContent=t(id,args)}
function applyLanguage(){document.documentElement.lang=language;document.title=t('title');$('language').value=language;
document.querySelectorAll('[data-i18n]').forEach(e=>e.textContent=t(e.dataset.i18n,JSON.parse(e.dataset.i18nArgs||'{}')));
document.querySelectorAll('[data-i18n-aria]').forEach(e=>e.setAttribute('aria-label',t(e.dataset.i18nAria)));
document.querySelectorAll('[data-label-key]').forEach(e=>e.dataset.label=t(e.dataset.labelKey))}
class UiError extends Error{constructor(id,args={}){super(t(id,args));this.id=id;this.args=args}}
function serverError(data,status){return Object.hasOwn(translations,data?.error)?new UiError(data.error):data?.message?new Error(data.message):new UiError('httpError',{status})}
$('language').onchange=()=>{language=$('language').value;try{localStorage.setItem('cydSdLanguage',language)}catch(e){}applyLanguage()};applyLanguage();
$('file').onchange=()=>{const file=$('file').files[0];if(file){delete $('filename').dataset.i18n;$('filename').textContent=file.name}else translated($('filename'),'noFile')};
$('theme').value=document.documentElement.dataset.theme||'auto';
$('theme').onchange=()=>{const theme=$('theme').value;if(theme==='auto')delete document.documentElement.dataset.theme;else document.documentElement.dataset.theme=theme;try{localStorage.setItem('cydSdTheme',theme)}catch(e){}};
const api=p=>p;function key(){return $('key').value}function headers(){return {'X-API-Key':key()}}
function show(text,ok=false){const m=$('message');m.hidden=false;m.className=ok?'ok':'error';if(text instanceof UiError)translated(m,text.id,text.args);else{delete m.dataset.i18n;m.textContent=text instanceof Error?text.message:text}}
function lock(value){busy=value;document.querySelectorAll('button,input[type=file]').forEach(e=>e.disabled=value)}
async function jsonRequest(url,options={}){options.headers={...headers(),...(options.headers||{})};let response;
try{response=await fetch(url,options)}catch(e){throw new UiError('network',{detail:e.message})}
let data;try{data=await response.json()}catch(e){throw new UiError('invalidResponse',{status:response.status})}
if(!response.ok||!data.ok)throw serverError(data,response.status);return data}
function childPath(name){return current==='/'?'/'+name:current+'/'+name}
function formatBytes(n){if(n<1024)return n+' B';for(const u of ['KiB','MiB','GiB']){n/=1024;if(n<1024)return n.toFixed(1)+' '+u}return n.toFixed(1)+' TiB'}
async function refresh(){if(!key())return show(new UiError('missingKey'));lock(true);try{sessionStorage.setItem('cydSdApiKey',key());
const health=await jsonRequest(api('/sd/api/health'));translated($('health'),health.mounted?'health':'unavailable',{card:health.card_type,free:formatBytes(health.free_bytes),total:formatBytes(health.total_bytes)});
const listing=await jsonRequest(api('/sd/api/list?path='+encodeURIComponent(current)+'&limit=100'));$('path').textContent=current;const body=$('entries');body.replaceChildren();
for(const entry of listing.entries){const tr=document.createElement('tr');tr.setAttribute('role','row');const name=document.createElement('td');const open=document.createElement('button');open.textContent=entry.name;open.className='entry-name';
open.onclick=()=>entry.type==='directory'?navigate(entry.path):download(entry.path,entry.name);name.append(open);const type=document.createElement('td');translated(type,entry.type==='directory'?'directory':'file');type.dataset.labelKey='type';type.dataset.label=t('type');
const size=document.createElement('td');size.textContent=entry.type==='file'?formatBytes(entry.size):'—';size.dataset.labelKey='size';size.dataset.label=t('size');const actions=document.createElement('td');actions.className='actions';
if(entry.type==='file'){const dl=document.createElement('button');translated(dl,'download');dl.onclick=()=>download(entry.path,entry.name);actions.append(dl)}
const del=document.createElement('button');translated(del,'remove');del.className='danger';del.onclick=()=>removeEntry(entry);actions.append(del);for(const cell of [name,type,size,actions])cell.setAttribute('role','cell');tr.append(name,type,size,actions);body.append(tr)}
if(!listing.entries.length){const tr=document.createElement('tr');tr.setAttribute('role','row');const td=document.createElement('td');td.setAttribute('role','cell');td.colSpan=4;td.className='empty';translated(td,'empty');tr.append(td);body.append(tr)}
if(listing.has_more)show(new UiError('more'));
}catch(e){show(e)}finally{lock(false)}}
function navigate(path){current=path;refresh()}function parent(){if(current==='/')return;current=current.slice(0,current.lastIndexOf('/'))||'/';refresh()}
async function upload(){const file=$('file').files[0];if(!file)return show(new UiError('selectFile'));const target=childPath(file.name);lock(true);$('progress').value=0;
await new Promise(resolve=>{const xhr=new XMLHttpRequest();xhr.open('POST','/sd/api/file?path='+encodeURIComponent(target)+'&overwrite='+$('overwrite').checked);xhr.setRequestHeader('X-API-Key',key());xhr.setRequestHeader('Content-Type','application/octet-stream');
xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=e.loaded/e.total*100};xhr.onerror=()=>{show(new UiError('uploadNetwork'));resolve()};xhr.onload=()=>{let d;try{d=JSON.parse(xhr.responseText)}catch(e){show(new UiError('invalidResponse',{status:xhr.status}));resolve();return}const ok=xhr.status>=200&&xhr.status<300&&d.ok;show(ok?new UiError('uploaded',{bytes:d.bytes}):serverError(d,xhr.status),ok);resolve()};xhr.send(file)});lock(false);await refresh()}
async function download(path,name){lock(true);try{let response;try{response=await fetch('/sd/api/file?path='+encodeURIComponent(path),{headers:headers()})}catch(e){throw new UiError('network',{detail:e.message})}if(!response.ok){let d;try{d=await response.json()}catch(e){}throw serverError(d,response.status)}
const blob=await response.blob();const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();URL.revokeObjectURL(url)}catch(e){show(e)}finally{lock(false)}}
async function removeEntry(entry){if(!confirm(t('confirmDelete',{path:entry.path})))return;lock(true);try{const kind=entry.type==='directory'?'directory':'file';await jsonRequest('/sd/api/'+kind+'?path='+encodeURIComponent(entry.path),{method:'DELETE'});show(new UiError('deleted',{path:entry.path}),true)}catch(e){show(e)}finally{lock(false);await refresh()}}
async function mkdir(){const name=prompt(t('directoryName'));if(!name)return;lock(true);try{const path=childPath(name);await jsonRequest('/sd/api/directory?path='+encodeURIComponent(path),{method:'POST'});show(new UiError('created',{path}),true)}catch(e){show(e)}finally{lock(false);await refresh()}}
$('connect').onclick=refresh;$('refresh').onclick=refresh;$('up').onclick=parent;$('upload').onclick=upload;$('mkdir').onclick=mkdir;
$('key').value=sessionStorage.getItem('cydSdApiKey')||'';if($('key').value)refresh();
</script></body></html>)HTML";

}  // namespace cyd_sd_http_server
}  // namespace esphome
