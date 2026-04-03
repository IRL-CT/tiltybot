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

// Helper to serve a LittleFS file with correct content type
esp_err_t serveFile(PsychicRequest *request, PsychicResponse *response, const char *path, const char *contentType) {
    File f = LittleFS.open(path);
    if (!f) {
        return response->send(404, "text/plain", "File not found");
    }
    String content = f.readString();
    f.close();
    return response->send(200, contentType, content.c_str());
}

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

    // --- HTTP routes (served from LittleFS) ---
    server.on("/", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/index.html", "text/html");
    });
    server.on("/drive.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/drive.html", "text/html");
    });
    server.on("/tilty.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/tilty.html", "text/html");
    });
    server.on("/2motor.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/2motor.html", "text/html");
    });
    server.on("/sound.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/sound.html", "text/html");
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
