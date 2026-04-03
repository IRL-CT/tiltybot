/*
 * TiltyBot — Unified Firmware
 *
 * Single firmware serving all control modes:
 *   - Drive: differential drive with virtual joystick
 *   - Tilty: pan/tilt head via phone gyroscope or sliders
 *   - 2-Motor: direct position control (0-360°)
 *
 * Switch between modes via the index page at https://192.168.4.1
 * Motor control mode switches automatically on WebSocket connect.
 *
 * Hardware: Waveshare ESP32-S3-Zero + 2x Dynamixel XL330
 * Flash with: pio run -e tiltybot -t upload
 */

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>
#include "XL330.h"

// ---- Pin config (Waveshare ESP32-S3-Zero) ----
#define RXD2 2
#define TXD2 1
#define RGB_LED 21
#define DRIVE_MODE 16
#define POSITION_MODE 3
#define BROADCAST 254
#define MOTOR1 1
#define MOTOR2 2

// ---- WiFi config ----
const char *ssid = "YOURGROUPNAME";
const char *password = "12345678";

// ---- Globals ----
PsychicHttpsServer server;
PsychicHttpServer httpRedirect;
PsychicWebSocketHandler driveWs;
PsychicWebSocketHandler tiltyWs;
PsychicWebSocketHandler twoMotorWs;
XL330 robot;
String serverCert;
String serverKey;
int prevM1 = 0;
int prevM2 = 0;
int currentMode = -1;
int targetM1 = 0;
int targetM2 = 0;
int pendingMode = -1;
volatile bool newData = false;

void setMotorMode(int mode) {
    // Stop any motion first
    if (currentMode == DRIVE_MODE) {
        robot.setJointSpeed(MOTOR1, 0);
        robot.setJointSpeed(MOTOR2, 0);
        delay(10);
    }
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setControlMode(BROADCAST, mode);
    delay(50);
    robot.TorqueON(BROADCAST);
    delay(50);
    prevM1 = 0;
    prevM2 = 0;
    currentMode = mode;
    Serial.printf("Motor mode -> %s\n", mode == DRIVE_MODE ? "DRIVE" : "POSITION");
}

// ---- Index page ----
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>TiltyBot</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
body{background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;padding:2em}
a{display:block;width:80vw;max-width:400px;margin:0.5em;padding:1em;background:#333;color:#fff;text-align:center;text-decoration:none;border-radius:8px;font-size:1.2em}
a:active{background:#555}
</style></head><body>
<h2>TiltyBot 🤖</h2>
<a href="/drive.html">🚗 Drive Mode</a>
<a href="/tilty.html">🎯 Tilty Mode</a>
<a href="/2motor.html">⚙️ 2-Motor Mode</a>
<a href="/sound.html">Sound Recorder</a>
</body></html>
)rawliteral";

// ---- Drive page ----
const char drive_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>TiltyBot Drive</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
html,body{margin:0;height:100%;overflow:hidden;background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center}
h3{margin:0.5em}
#status{color:#0f0;font-size:0.8em}
label{margin:1em}
canvas{border:2px solid #444;border-radius:50%;touch-action:none}
.back{position:fixed;top:10px;left:10px;color:#888;text-decoration:none}
</style></head><body>
<a class="back" href="/">← back</a>
<h3>Drive 🚗</h3>
<p id="status">Connecting...</p>
<label>Active: <input id="active" type="checkbox"></label>
<canvas id="joy" width="300" height="300"></canvas>
<script>
var ws,active=document.getElementById('active'),statusEl=document.getElementById('status');
var canvas=document.getElementById('joy'),ctx=canvas.getContext('2d');
var cx=150,cy=150,r=130,jx=0,jy=0,touching=false;
function connect(){
  var proto=location.protocol==='https:'?'wss:':'ws:';
  ws=new WebSocket(proto+'//'+location.host+'/ws/drive');
  ws.onopen=function(){statusEl.textContent='Connected';statusEl.style.color='#0f0'};
  ws.onclose=function(){statusEl.textContent='Disconnected';statusEl.style.color='#f00';setTimeout(connect,2000)};
}
connect();
function draw(){
  ctx.clearRect(0,0,300,300);
  ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.strokeStyle='#444';ctx.lineWidth=2;ctx.stroke();
  ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);ctx.fillStyle='#666';ctx.fill();
  var px=cx+jx*r/100,py=cy-jy*r/100;
  ctx.beginPath();ctx.arc(px,py,25,0,Math.PI*2);ctx.fillStyle='#4af';ctx.fill();
}
draw();
function getPos(e){
  var rect=canvas.getBoundingClientRect(),t=e.touches?e.touches[0]:e;
  var dx=t.clientX-rect.left-cx,dy=-(t.clientY-rect.top-cy),dist=Math.sqrt(dx*dx+dy*dy);
  if(dist>r){dx=dx/dist*r;dy=dy/dist*r}
  return{x:Math.round(dx/r*100),y:Math.round(dy/r*100)};
}
canvas.addEventListener('pointerdown',function(e){touching=true;var p=getPos(e);jx=p.x;jy=p.y;draw();send()});
canvas.addEventListener('pointermove',function(e){if(!touching)return;var p=getPos(e);jx=p.x;jy=p.y;draw();send()});
canvas.addEventListener('pointerup',function(){touching=false;jx=0;jy=0;draw();send()});
canvas.addEventListener('pointercancel',function(){touching=false;jx=0;jy=0;draw();send()});
function send(){if(ws&&ws.readyState===1&&active.checked)ws.send(JSON.stringify({b:jx,g:jy}))}
</script></body></html>
)rawliteral";

// ---- Tilty page ----
const char tilty_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>TiltyBot Tilty</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
html,body{margin:0;height:100%;overflow:hidden;background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center}
h3{margin:0.5em}
h4{margin:0.3em;font-weight:normal}
#status{color:#0f0;font-size:0.8em}
label{margin:0.5em}
input[type=range]{width:80vw;max-width:400px}
.back{position:fixed;top:10px;left:10px;color:#888;text-decoration:none}
.vert-container{height:50vh;display:flex;align-items:center;justify-content:center}
.vert-container input{transform:rotate(270deg);width:50vh}
</style></head><body>
<a class="back" href="/">← back</a>
<h3>Tilty 🎯</h3>
<p id="status">Connecting...</p>
<label>Gyro: <input id="active" type="checkbox"></label>
<h4>Pan: <span id="pan">0</span></h4>
<h4>Tilt: <span id="tilt">0</span></h4>
<div class="vert-container"><input id="tiltIn" type="range" min="-179" max="179" value="90"></div>
<input id="panIn" type="range" min="-89" max="89" value="0">
<script>
var ws,active=document.getElementById('active'),statusEl=document.getElementById('status');
var panEl=document.getElementById('pan'),tiltEl=document.getElementById('tilt');
var panIn=document.getElementById('panIn'),tiltIn=document.getElementById('tiltIn');
var b=tiltIn.value,g=panIn.value;
function connect(){
  var proto=location.protocol==='https:'?'wss:':'ws:';
  ws=new WebSocket(proto+'//'+location.host+'/ws/tilty');
  ws.onopen=function(){statusEl.textContent='Connected';statusEl.style.color='#0f0'};
  ws.onclose=function(){statusEl.textContent='Disconnected';statusEl.style.color='#f00';setTimeout(connect,2000)};
}
connect();
function send(){if(ws&&ws.readyState===1)ws.send(JSON.stringify({b:b,g:g}))}
tiltIn.oninput=function(){b=tiltIn.value;tiltEl.textContent=b;send()};
panIn.oninput=function(){g=panIn.value;panEl.textContent=g;send()};
active.onchange=function(){
  if(active.checked){
    panIn.disabled=true;tiltIn.disabled=true;
    if(typeof DeviceMotionEvent!=='undefined'&&typeof DeviceMotionEvent.requestPermission==='function'){
      DeviceMotionEvent.requestPermission().then(function(s){
        if(s==='granted')window.addEventListener('deviceorientation',handleOri,true);
      });
    }else{window.addEventListener('deviceorientation',handleOri,true)}
  }else{panIn.disabled=false;tiltIn.disabled=false}
};
function handleOri(e){
  if(!active.checked)return;
  b=e.beta.toFixed(3);g=e.gamma.toFixed(3);
  tiltEl.textContent=b;panEl.textContent=g;send();
}
try{navigator.wakeLock.request('screen')}catch(e){}
</script></body></html>
)rawliteral";

// ---- 2-Motor page ----
const char twomotor_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>TiltyBot 2-Motor</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
html,body{margin:0;height:100%;overflow:hidden;background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center}
h3{margin:0.5em}
h4{margin:0.3em;font-weight:normal}
#status{color:#0f0;font-size:0.8em}
input[type=range]{width:80vw;max-width:400px;margin:0.5em}
.back{position:fixed;top:10px;left:10px;color:#888;text-decoration:none}
</style></head><body>
<a class="back" href="/">← back</a>
<h3>2-Motor ⚙️</h3>
<p id="status">Connecting...</p>
<h4>Motor 1: <span id="m1v">180</span>°</h4>
<input id="m1" type="range" min="0" max="360" value="180">
<h4>Motor 2: <span id="m2v">180</span>°</h4>
<input id="m2" type="range" min="0" max="360" value="180">
<script>
var ws,statusEl=document.getElementById('status');
var m1=document.getElementById('m1'),m2=document.getElementById('m2');
var m1v=document.getElementById('m1v'),m2v=document.getElementById('m2v');
function connect(){
  var proto=location.protocol==='https:'?'wss:':'ws:';
  ws=new WebSocket(proto+'//'+location.host+'/ws/2motor');
  ws.onopen=function(){statusEl.textContent='Connected';statusEl.style.color='#0f0'};
  ws.onclose=function(){statusEl.textContent='Disconnected';statusEl.style.color='#f00';setTimeout(connect,2000)};
}
connect();
function send(){if(ws&&ws.readyState===1)ws.send(JSON.stringify({b:m1.value,g:m2.value}))}
m1.oninput=function(){m1v.textContent=m1.value;send()};
m2.oninput=function(){m2v.textContent=m2.value;send()};
</script></body></html>
)rawliteral";

// ---- Sound page ----
const char sound_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>TiltyBot Sound</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
html,body{margin:0;min-height:100%;background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;padding:1em}
h3{margin:0.5em}
.back{position:fixed;top:10px;left:10px;color:#888;text-decoration:none}
button{background:#333;color:#fff;border:1px solid #555;border-radius:8px;padding:0.6em 1.2em;font-size:0.9em;margin:0.2em;cursor:pointer}
button:active{background:#555}
button:disabled{opacity:0.3}
.recording{background:red!important;color:#000}
.accent{background:#1a6b3a;border-color:#2a8b4a}
#timer{margin-left:1em;font-size:1.2em}
.controls{display:flex;align-items:center;margin:0.8em 0;flex-wrap:wrap;justify-content:center;gap:0.3em}
.upload-label{background:#225;border:2px dashed #558;border-radius:8px;padding:0.8em 1.5em;font-size:0.95em;cursor:pointer;text-align:center;margin:0.5em 0}
.upload-label:active{background:#337}
.clip{background:#222;border-radius:8px;padding:1em;margin:0.5em 0;width:85vw;max-width:420px}
.clip .name{margin:0.3em 0;font-weight:bold;font-size:1.05em}
.clip audio{width:100%;margin:0.3em 0}
.clip .delete{background:#600;font-size:0.8em;padding:0.4em 0.8em}
.trim-row{display:flex;align-items:center;gap:0.4em;margin:0.4em 0;flex-wrap:wrap}
.trim-row input[type=number]{width:4.5em;background:#333;color:#fff;border:1px solid #555;border-radius:4px;padding:0.3em;font-size:0.9em;text-align:center}
.trim-row span{font-size:0.85em;color:#aaa}
.dur{font-size:0.8em;color:#888;margin:0.2em 0}
</style></head><body>
<a class="back" href="/">← back</a>
<h3>Sound</h3>

<div class="controls">
  <button class="record">Record</button>
  <button class="stop" disabled>Stop</button>
  <span id="timer">00:00</span>
</div>

<label class="upload-label">
  Load audio files
  <input type="file" id="fileIn" accept="audio/*" multiple hidden>
</label>

<section class="sound-clips"></section>

<script>
var soundClips=document.querySelector('.sound-clips');
var actx=new (window.AudioContext||window.webkitAudioContext)();

function fmt(t){var m=Math.floor(t/60),s=(t%60).toFixed(1);return m+':'+s.padStart(4,'0')}

function encodeWav(buf){
  var ch=buf.numberOfChannels,sr=buf.sampleRate,len=buf.length;
  var out=new DataView(new ArrayBuffer(44+len*ch*2));
  function s(o,v){for(var i=0;i<v.length;i++)out.setUint8(o+i,v.charCodeAt(i))}
  s(0,'RIFF');out.setUint32(4,36+len*ch*2,true);s(8,'WAVE');
  s(12,'fmt ');out.setUint32(16,16,true);out.setUint16(20,1,true);
  out.setUint16(22,ch,true);out.setUint32(24,sr,true);
  out.setUint32(28,sr*ch*2,true);out.setUint16(32,ch*2,true);out.setUint16(34,16,true);
  s(36,'data');out.setUint32(40,len*ch*2,true);
  var off=44;
  for(var i=0;i<len;i++)for(var c=0;c<ch;c++){
    var v=Math.max(-1,Math.min(1,buf.getChannelData(c)[i]));
    out.setInt16(off,v<0?v*32768:v*32767,true);off+=2;
  }
  return new Blob([out],{type:'audio/wav'});
}

function addClip(blob,name){
  var div=document.createElement('div');div.className='clip';
  var p=document.createElement('p');p.className='name';p.textContent=name;
  var audio=document.createElement('audio');audio.controls=true;
  var dur=document.createElement('div');dur.className='dur';dur.textContent='Loading...';
  audio.onloadedmetadata=function(){
    var d=audio.duration;
    dur.textContent=fmt(d);
    endIn.value=d.toFixed(1);endIn.max=d.toFixed(1);
    startIn.max=d.toFixed(1);
  };
  audio.src=URL.createObjectURL(blob);
  audio.load();

  var row=document.createElement('div');row.className='trim-row';
  var startIn=document.createElement('input');startIn.type='number';startIn.value='0';startIn.min='0';startIn.step='0.1';
  var endIn=document.createElement('input');endIn.type='number';endIn.value='0';endIn.min='0';endIn.step='0.1';
  var markS=document.createElement('button');markS.textContent='[ Set';
  var markE=document.createElement('button');markE.textContent='Set ]';
  var trimBtn=document.createElement('button');trimBtn.textContent='Trim';trimBtn.className='accent';
  var sep=document.createElement('span');sep.textContent='→';

  markS.onclick=function(){startIn.value=audio.currentTime.toFixed(1)};
  markE.onclick=function(){endIn.value=audio.currentTime.toFixed(1)};

  trimBtn.onclick=function(){
    var s=parseFloat(startIn.value)||0,e=parseFloat(endIn.value)||0;
    if(e<=s){alert('End must be after start');return}
    trimBtn.disabled=true;trimBtn.textContent='Trimming...';
    fetch(audio.src).then(function(r){return r.arrayBuffer()}).then(function(ab){
      return actx.decodeAudioData(ab);
    }).then(function(decoded){
      var sr=decoded.sampleRate,ch=decoded.numberOfChannels;
      var ss=Math.floor(s*sr),se=Math.min(Math.floor(e*sr),decoded.length);
      var newLen=se-ss;
      if(newLen<=0){alert('Invalid range');trimBtn.disabled=false;trimBtn.textContent='Trim';return}
      var offCtx=new OfflineAudioContext(ch,newLen,sr);
      var newBuf=offCtx.createBuffer(ch,newLen,sr);
      for(var c=0;c<ch;c++){
        var src=decoded.getChannelData(c),dst=newBuf.getChannelData(c);
        for(var i=0;i<newLen;i++)dst[i]=src[ss+i];
      }
      var wavBlob=encodeWav(newBuf);
      var tName=name+' ['+fmt(s)+'-'+fmt(e)+']';
      addClip(wavBlob,tName);
      trimBtn.disabled=false;trimBtn.textContent='Trim';
    }).catch(function(err){
      console.error(err);alert('Trim failed: '+err.message);
      trimBtn.disabled=false;trimBtn.textContent='Trim';
    });
  };

  row.appendChild(markS);row.appendChild(startIn);row.appendChild(sep);
  row.appendChild(endIn);row.appendChild(markE);row.appendChild(trimBtn);

  var del=document.createElement('button');del.textContent='Delete';del.className='delete';
  del.onclick=function(){URL.revokeObjectURL(audio.src);div.remove()};

  div.appendChild(p);div.appendChild(audio);div.appendChild(dur);
  div.appendChild(row);div.appendChild(del);
  soundClips.appendChild(div);
  div.scrollIntoView({behavior:'smooth'});
}

/* ---- Recording ---- */
var record=document.querySelector('.record'),stop=document.querySelector('.stop');
var timerEl=document.querySelector('#timer'),startTime,timerInterval;
if(navigator.mediaDevices&&navigator.mediaDevices.getUserMedia){
  var chunks=[];
  navigator.mediaDevices.getUserMedia({audio:true}).then(function(stream){
    var mr=new MediaRecorder(stream);
    record.onclick=function(){
      mr.start();record.classList.add('recording');
      startTime=Date.now();timerInterval=setInterval(function(){
        var e=new Date(Date.now()-startTime);
        timerEl.textContent=e.getUTCMinutes().toString().padStart(2,'0')+':'+e.getUTCSeconds().toString().padStart(2,'0');
      },1000);
      stop.disabled=false;record.disabled=true;
    };
    stop.onclick=function(){
      mr.stop();record.classList.remove('recording');
      stop.disabled=true;record.disabled=false;clearInterval(timerInterval);
    };
    mr.onstop=function(){
      timerEl.textContent='00:00';
      var blob=new Blob(chunks,{type:mr.mimeType});chunks=[];
      var n=prompt('Name your clip:','recording');
      addClip(blob,n||'Recording');
    };
    mr.ondataavailable=function(e){chunks.push(e.data)};
  },function(err){console.log('Mic error:',err);record.disabled=true});
}else{record.disabled=true}

/* ---- File upload ---- */
document.getElementById('fileIn').onchange=function(e){
  var files=e.target.files;
  for(var i=0;i<files.length;i++){
    (function(f){
      addClip(f,f.name.replace(/\.[^.]+$/,''));
    })(files[i]);
  }
  e.target.value='';
};
</script></body></html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== TiltyBot ===");

    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

    // LittleFS + SSL certs
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed!");
    } else {
        File certFile = LittleFS.open("/server.crt");
        File keyFile = LittleFS.open("/server.key");
        if (certFile && keyFile) {
            serverCert = certFile.readString();
            serverKey = keyFile.readString();
            certFile.close();
            keyFile.close();
            server.setCertificate(serverCert.c_str(), serverKey.c_str());
            Serial.println("SSL certs loaded");
        } else {
            Serial.println("SSL cert/key not found!");
        }
    }

    // WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    delay(500);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // --- Drive WebSocket ---
    driveWs.onOpen([](PsychicWebSocketClient *client) {
        Serial.println("Drive client connected");
        pendingMode = DRIVE_MODE;
    });
    driveWs.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
        String msg = String((char *)frame->payload, frame->len);
        JsonDocument doc;
        deserializeJson(doc, msg);
        int xRaw = doc["b"].as<int>();
        int yRaw = doc["g"].as<int>();
        int x = map(xRaw, -100, 100, -885, 885);
        int y = map(yRaw, -100, 100, -885, 885);
        targetM1 = constrain(x + y, -855, 855);
        targetM2 = constrain(x - y, -855, 855);
        newData = true;
        return ESP_OK;
    });
    driveWs.onClose([](PsychicWebSocketClient *client) {
        targetM1 = 0;
        targetM2 = 0;
        newData = true;
        Serial.println("Drive client disconnected");
    });
    server.on("/ws/drive", &driveWs);

    // --- Tilty WebSocket ---
    tiltyWs.onOpen([](PsychicWebSocketClient *client) {
        Serial.println("Tilty client connected");
        pendingMode = POSITION_MODE;
    });
    tiltyWs.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
        String msg = String((char *)frame->payload, frame->len);
        JsonDocument doc;
        deserializeJson(doc, msg);
        float bVal = doc["b"].as<float>();
        float gVal = doc["g"].as<float>();
        targetM1 = int(constrain(bVal / 0.088, 70, 2150));
        targetM2 = int(map((int)gVal, -89, 89, 1024, 3072));
        newData = true;
        return ESP_OK;
    });
    server.on("/ws/tilty", &tiltyWs);

    // --- 2-Motor WebSocket ---
    twoMotorWs.onOpen([](PsychicWebSocketClient *client) {
        Serial.println("2-Motor client connected");
        pendingMode = POSITION_MODE;
    });
    twoMotorWs.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
        String msg = String((char *)frame->payload, frame->len);
        JsonDocument doc;
        deserializeJson(doc, msg);
        int m1Deg = doc["b"].as<int>();
        int m2Deg = doc["g"].as<int>();
        targetM1 = map(m1Deg, 0, 360, 0, 4095);
        targetM2 = map(m2Deg, 0, 360, 0, 4095);
        newData = true;
        return ESP_OK;
    });
    server.on("/ws/2motor", &twoMotorWs);

    // --- HTTP routes ---
    server.on("/", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", index_html);
    });
    server.on("/drive.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", drive_html);
    });
    server.on("/tilty.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", tilty_html);
    });
    server.on("/2motor.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", twomotor_html);
    });
    server.on("/sound.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", sound_html);
    });

    server.begin();
    Serial.println("HTTPS server started on port 443");

    // HTTP redirect server on port 80
    httpRedirect.config.ctrl_port = 20424; // different control port from main server
    httpRedirect.onNotFound([](PsychicRequest *request, PsychicResponse *response) {
        String url = "https://" + request->host() + request->url();
        response->setCode(301);
        response->addHeader("Location", url.c_str());
        return response->send();
    });
    httpRedirect.begin();
    Serial.println("HTTP->HTTPS redirect on port 80");

    // Init motors
    Serial2.flush();
    robot.begin(Serial2);
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setControlMode(BROADCAST, POSITION_MODE);
    delay(50);
    robot.TorqueON(BROADCAST);
    delay(50);
    currentMode = POSITION_MODE;
    robot.LEDON(MOTOR1);
    robot.LEDON(MOTOR2);
    delay(500);
    robot.LEDOFF(MOTOR1);
    robot.LEDOFF(MOTOR2);
    Serial.println("Ready. Open https://192.168.4.1");
}

void loop() {
    // Handle mode switch on main thread
    if (pendingMode >= 0) {
        setMotorMode(pendingMode);
        pendingMode = -1;
    }

    // Handle motor commands on main thread
    if (newData) {
        newData = false;
        if (currentMode == DRIVE_MODE) {
            if (abs(targetM1 - prevM1) > 5 || targetM1 == 0) {
                robot.setJointSpeed(MOTOR1, targetM1);
                prevM1 = targetM1;
                delay(5);
            }
            if (abs(targetM2 - prevM2) > 5 || targetM2 == 0) {
                robot.setJointSpeed(MOTOR2, targetM2);
                prevM2 = targetM2;
                delay(5);
            }
        } else {
            if (abs(targetM1 - prevM1) > 5) {
                robot.setJointPosition(MOTOR1, targetM1);
                prevM1 = targetM1;
                delay(1);
            }
            if (abs(targetM2 - prevM2) > 5) {
                robot.setJointPosition(MOTOR2, targetM2);
                prevM2 = targetM2;
                delay(1);
            }
        }
    }

    delay(5);
}
