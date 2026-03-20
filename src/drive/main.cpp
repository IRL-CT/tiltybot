/*
 * TiltyBot — Drive Mode
 *
 * Differential drive with virtual joystick over WebSocket.
 * Two XL330 motors in velocity (drive) mode.
 *
 * Flash with: pio run -e drive -t upload
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
#define BROADCAST 254
#define MOTOR1 1
#define MOTOR2 2

// ---- WiFi config ----
const char *ssid = "YOURGROUPNAME";
const char *password = "12345678";

// ---- Globals ----
PsychicHttpsServer server;
PsychicWebSocketHandler wsHandler;
XL330 robot;
String serverCert;
String serverKey;
int prevM1 = 0;
int prevM2 = 0;

// ---- Drive page HTML ----
const char drive_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<title>TiltyBot Drive</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
html,body{margin:0;height:100%;overflow:hidden;background:#111;color:#fff;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center}
h3{margin:0.5em}
#status{color:#0f0;font-size:0.8em}
label{margin:1em}
canvas{border:2px solid #444;border-radius:50%;touch-action:none}
</style></head><body>
<h3>TiltyBot Drive 🤖</h3>
<p id="status">Connecting...</p>
<label>Active: <input id="active" type="checkbox"></label>
<canvas id="joy" width="300" height="300"></canvas>
<script>
var ws, active=document.getElementById('active'), status=document.getElementById('status');
var canvas=document.getElementById('joy'), ctx=canvas.getContext('2d');
var cx=150,cy=150,r=130,jx=0,jy=0,touching=false;

function connect(){
  var proto=location.protocol==='https:'?'wss:':'ws:';
  ws=new WebSocket(proto+'//'+location.host+'/ws');
  ws.onopen=function(){status.textContent='Connected';status.style.color='#0f0'};
  ws.onclose=function(){status.textContent='Disconnected';status.style.color='#f00';setTimeout(connect,2000)};
}
connect();

function draw(){
  ctx.clearRect(0,0,300,300);
  ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.strokeStyle='#444';ctx.lineWidth=2;ctx.stroke();
  ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);ctx.fillStyle='#666';ctx.fill();
  var px=cx+jx*r/100, py=cy-jy*r/100;
  ctx.beginPath();ctx.arc(px,py,25,0,Math.PI*2);ctx.fillStyle='#4af';ctx.fill();
}
draw();

function getPos(e){
  var rect=canvas.getBoundingClientRect();
  var t=e.touches?e.touches[0]:e;
  var dx=t.clientX-rect.left-cx, dy=-(t.clientY-rect.top-cy);
  var dist=Math.sqrt(dx*dx+dy*dy);
  if(dist>r){dx=dx/dist*r;dy=dy/dist*r}
  return{x:Math.round(dx/r*100),y:Math.round(dy/r*100)};
}

canvas.addEventListener('pointerdown',function(e){touching=true;var p=getPos(e);jx=p.x;jy=p.y;draw();send()});
canvas.addEventListener('pointermove',function(e){if(!touching)return;var p=getPos(e);jx=p.x;jy=p.y;draw();send()});
canvas.addEventListener('pointerup',function(){touching=false;jx=0;jy=0;draw();send()});
canvas.addEventListener('pointercancel',function(){touching=false;jx=0;jy=0;draw();send()});

function send(){
  if(ws&&ws.readyState===1&&active.checked){
    ws.send(JSON.stringify({b:jx,g:jy}));
  }
}
</script></body></html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== TiltyBot Drive Mode ===");

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

    // WebSocket
    wsHandler.onOpen([](PsychicWebSocketClient *client) {
        Serial.printf("WS client #%u connected\n", client->socket());
    });

    wsHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
        String msg = String((char *)frame->payload, frame->len);
        JsonDocument doc;
        deserializeJson(doc, msg);

        int xRaw = doc["b"] | 0;
        int yRaw = doc["g"] | 0;
        int x = map(xRaw, -100, 100, -885, 885);
        int y = map(yRaw, -100, 100, -885, 885);
        int speedM1 = constrain(x + y, -855, 855);
        int speedM2 = constrain(x - y, -855, 855);

        if (abs(speedM1 - prevM1) > 5 || speedM1 == 0) {
            robot.setJointSpeed(MOTOR1, speedM1);
            prevM1 = speedM1;
            delay(10);
        }
        if (abs(speedM2 - prevM2) > 5 || speedM2 == 0) {
            robot.setJointSpeed(MOTOR2, speedM2);
            prevM2 = speedM2;
            delay(10);
        }

        return ESP_OK;
    });

    wsHandler.onClose([](PsychicWebSocketClient *client) {
        Serial.printf("WS client #%u disconnected\n", client->socket());
    });

    server.on("/ws", &wsHandler);

    server.on("/", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        response->setCode(302);
        response->addHeader("Location", "/drive.html");
        return response->send();
    });

    server.on("/drive.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return response->send(200, "text/html", drive_html);
    });

    server.begin();
    Serial.println("HTTPS server started");

    // Init motors
    Serial2.flush();
    robot.begin(Serial2);
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setControlMode(BROADCAST, DRIVE_MODE);
    delay(50);
    robot.TorqueON(BROADCAST);
    delay(50);
    robot.LEDON(MOTOR1);
    robot.LEDON(MOTOR2);
    delay(500);
    robot.LEDOFF(MOTOR1);
    robot.LEDOFF(MOTOR2);
    Serial.println("Motors ready. Open https://192.168.4.1");
}

void loop() {
    delay(5);
}
