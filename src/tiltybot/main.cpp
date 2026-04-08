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
#include <esp_wifi.h>
#include <esp_now.h>
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
#ifndef BOT_SSID
#define BOT_SSID "YOURGROUPNAME"
#endif
const char *ssid = BOT_SSID;
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
// Store last received values for logging from main loop
volatile float lastBVal = 0, lastGVal = 0;

volatile uint32_t wsFrameCount = 0; // frames received by callback
volatile uint32_t lastSeqNum = 0;   // last sequence number from client
uint32_t loopProcessCount = 0;      // frames processed by main loop
unsigned long lastStatsTime = 0;

// ---- Calibration config ----
// Position limits relative to home (2048 after homing offset)
// ±1024 from center = ±90 degrees range
#define CALIB_CENTER 2048
#define CALIB_MIN    1024
#define CALIB_MAX    3072

// Read position with echo-skip (half-duplex bus echoes TX on RX)
int32_t readMotorPosition(int id) {
    while (Serial2.available()) Serial2.read();
    delay(10);
    int sent = robot.RXsendPacket(id, 132, 4); // 132 = Present Position
    Serial2.flush();
    delay(20);
    // Skip TX echo
    unsigned long start = millis();
    int skipped = 0;
    while (skipped < sent && millis() - start < 200) {
        if (Serial2.available()) { Serial2.read(); skipped++; }
    }
    // Parse response
    unsigned char buffer[64];
    if (robot.readPacket(buffer, 64) > 0) {
        XL330::Packet p(buffer, 64);
        if (p.isValid() && p.getParameterCount() >= 3 && p.getInstruction() == 0x55) {
            return (p.getParameter(1)) |
                   (p.getParameter(2) << 8) |
                   (p.getParameter(3) << 16) |
                   (p.getParameter(4) << 24);
        }
    }
    return -1;
}

// ---- Puppet mode ----
enum PuppetState { PUPPET_IDLE, PUPPET_CONTROLLING, PUPPET_FOLLOWING };
volatile PuppetState puppetState = PUPPET_IDLE;
uint8_t puppetPairId = 0;
unsigned long lastPuppetRx = 0;

struct __attribute__((packed)) PuppetPacket {
    uint8_t pairId;
    float tilt;  // degrees from center
    float pan;   // degrees from center
};

// ESP-NOW receive callback
volatile unsigned long puppetRxCount = 0;
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (puppetState != PUPPET_FOLLOWING) {
        if (Serial) Serial.printf("ESP-NOW rx DROPPED: len=%d state=%d\n", len, puppetState);
        return;
    }
    if (len != sizeof(PuppetPacket)) return;
    PuppetPacket *pkt = (PuppetPacket *)data;
    if (pkt->pairId != puppetPairId) {
        if (Serial) Serial.printf("ESP-NOW rx WRONG PAIR: got=%d want=%d\n", pkt->pairId, puppetPairId);
        return;
    }
    // Convert degrees back to motor positions using OUR calibration
    targetM1 = constrain(CALIB_CENTER + int(pkt->tilt / 0.088f), CALIB_MIN, CALIB_MAX);
    targetM2 = constrain(CALIB_CENTER + int(pkt->pan / 0.088f), CALIB_MIN, CALIB_MAX);
    newData = true;
    lastPuppetRx = millis();
    puppetRxCount++;
}

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
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
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

    // WiFi AP only (ESP-NOW works in AP mode too)
    // AP_STA causes STA-side scanning that creates multi-second gaps
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    delay(500);
    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // ESP-NOW init
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
    } else {
        esp_now_register_recv_cb(onEspNowRecv);
        // Add broadcast peer
        esp_now_peer_info_t peer = {};
        memset(peer.peer_addr, 0xFF, 6);
        peer.channel = 0;
        peer.encrypt = false;
        peer.ifidx = WIFI_IF_AP;
        esp_now_add_peer(&peer);
        Serial.println("ESP-NOW ready");
    }

    // --- Drive WebSocket ---
    driveWs.onOpen([](PsychicWebSocketClient *client) {
        Serial.println("Drive client connected");
        puppetState = PUPPET_IDLE;
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
        puppetState = PUPPET_IDLE;
        pendingMode = POSITION_MODE;
    });
    tiltyWs.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
        String msg = String((char *)frame->payload, frame->len);
        JsonDocument doc;
        deserializeJson(doc, msg);
        float tilt = doc["b"].as<float>(); // elevation: -90 to +90
        float pan = doc["g"].as<float>();   // azimuth: -180 to +180
        // Both axes: 0° = center (2048), 0.088 deg/step
        int m1 = CALIB_CENTER + int(tilt / 0.088f);
        m1 = constrain(m1, CALIB_MIN, CALIB_MAX);
        int m2 = CALIB_CENTER + int(pan / 0.088f);
        m2 = constrain(m2, CALIB_MIN, CALIB_MAX);
        targetM1 = m1;
        targetM2 = m2;
        lastBVal = tilt;
        lastGVal = pan;
        lastSeqNum = doc["seq"] | 0;
        newData = true;
        wsFrameCount++;
        return ESP_OK;
    });
    server.on("/ws/tilty", &tiltyWs);

    // --- 2-Motor WebSocket ---
    twoMotorWs.onOpen([](PsychicWebSocketClient *client) {
        Serial.println("2-Motor client connected");
        puppetState = PUPPET_IDLE;
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
    server.on("/puppet.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/puppet.html", "text/html");
    });
    server.on("/calibrate.html", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        return serveFile(request, response, "/calibrate.html", "text/html");
    });

    // --- Puppet API ---
    server.on("/api/puppet/start", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        JsonDocument doc;
        deserializeJson(doc, request->body());
        int pairId = doc["pairId"].as<int>();
        String role = doc["role"].as<String>();
        puppetPairId = pairId;

        if (role == "controller") {
            // Torque off so user can move by hand
            robot.TorqueOFF(BROADCAST);
            delay(50);
            currentMode = -1;
            puppetState = PUPPET_CONTROLLING;
            Serial.printf("Puppet: CONTROLLER, pair %d\n", pairId);
        } else {
            // Puppet: position mode, torque on, ready to receive
            setMotorMode(POSITION_MODE);
            lastPuppetRx = millis();
            puppetState = PUPPET_FOLLOWING;
            Serial.printf("Puppet: FOLLOWING, pair %d\n", pairId);
        }
        return response->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/puppet/stop", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        puppetState = PUPPET_IDLE;
        setMotorMode(POSITION_MODE);
        Serial.println("Puppet: IDLE");
        return response->send(200, "application/json", "{\"ok\":true}");
    });

    // --- Calibration API ---
    server.on("/api/calibrate/release", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        if (Serial) Serial.println("CALIB: releasing motors");
        robot.TorqueOFF(BROADCAST);
        delay(50);
        currentMode = -1;
        if (Serial) Serial.println("CALIB: motors released");
        return response->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/calibrate/read", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        int32_t m1 = readMotorPosition(MOTOR1);
        int32_t m2 = readMotorPosition(MOTOR2);
        if (Serial) Serial.printf("CALIB: read m1=%d m2=%d\n", m1, m2);
        JsonDocument doc;
        doc["m1"] = m1;
        doc["m2"] = m2;
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    server.on("/api/calibrate/set", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        if (Serial) Serial.println("CALIB: starting calibration");

        // Step 1: Clear existing offset AND limits to get raw positions
        robot.TorqueOFF(BROADCAST);
        delay(50);
        if (Serial) Serial.println("CALIB: clearing old offsets and limits");
        robot.setHomingOffset(MOTOR1, 0);
        delay(50);
        robot.setHomingOffset(MOTOR2, 0);
        delay(50);
        robot.setMinPositionLimit(MOTOR1, 0);
        delay(50);
        robot.setMaxPositionLimit(MOTOR1, 4095);
        delay(50);
        robot.setMinPositionLimit(MOTOR2, 0);
        delay(50);
        robot.setMaxPositionLimit(MOTOR2, 4095);
        delay(50);
        // Cycle torque for EEPROM changes to take effect
        robot.TorqueON(BROADCAST);
        delay(100);
        robot.TorqueOFF(BROADCAST);
        delay(100);

        // Step 2: Read raw positions
        int32_t raw1 = readMotorPosition(MOTOR1);
        int32_t raw2 = readMotorPosition(MOTOR2);
        if (Serial) Serial.printf("CALIB: raw positions m1=%d m2=%d\n", raw1, raw2);
        if (raw1 < 0 || raw2 < 0) {
            if (Serial) Serial.println("CALIB: ERROR - cannot read motors");
            return response->send(200, "application/json", "{\"ok\":false,\"error\":\"Cannot read motors\"}");
        }

        // Step 3: Compute offsets
        int32_t offset1 = CALIB_CENTER - raw1;
        int32_t offset2 = CALIB_CENTER - raw2;
        if (Serial) Serial.printf("CALIB: offsets needed: m1=%d m2=%d\n", offset1, offset2);

        // Step 4: Validate range — Position Control Mode ignores offsets outside ±1024
        bool m1ok = (offset1 >= -1024 && offset1 <= 1024);
        bool m2ok = (offset2 >= -1024 && offset2 <= 1024);
        if (!m1ok || !m2ok) {
            if (Serial) Serial.printf("CALIB: ERROR - offset out of range [-1024,1024]\n");
            JsonDocument doc;
            doc["ok"] = false;
            String err = "";
            if (!m1ok) {
                int degOff = abs(offset1 - (offset1 > 0 ? 1024 : -1024)) * 88 / 1000;
                String dir = offset1 > 0 ? "counterclockwise" : "clockwise";
                err += "Motor 1 (tilt): rotate horn ~" + String(degOff) + " deg " + dir + " (viewed from shaft). ";
            }
            if (!m2ok) {
                int degOff = abs(offset2 - (offset2 > 0 ? 1024 : -1024)) * 88 / 1000;
                String dir = offset2 > 0 ? "counterclockwise" : "clockwise";
                err += "Motor 2 (pan): rotate horn ~" + String(degOff) + " deg " + dir + " (viewed from shaft). ";
            }
            err += "Remove horn, reposition on spline, reattach, then recalibrate.";
            doc["error"] = err;
            doc["raw1"] = raw1;
            doc["raw2"] = raw2;
            doc["offset1"] = offset1;
            doc["offset2"] = offset2;
            String out;
            serializeJson(doc, out);
            return response->send(200, "application/json", out.c_str());
        }

        // Step 5: Write offsets
        if (Serial) Serial.println("CALIB: writing homing offsets");
        robot.setHomingOffset(MOTOR1, offset1);
        delay(50);
        robot.setHomingOffset(MOTOR2, offset2);
        delay(50);

        // Step 6: Set position limits
        if (Serial) Serial.printf("CALIB: writing position limits [%d, %d]\n", CALIB_MIN, CALIB_MAX);
        robot.setMinPositionLimit(MOTOR1, CALIB_MIN);
        delay(50);
        robot.setMaxPositionLimit(MOTOR1, CALIB_MAX);
        delay(50);
        robot.setMinPositionLimit(MOTOR2, CALIB_MIN);
        delay(50);
        robot.setMaxPositionLimit(MOTOR2, CALIB_MAX);
        delay(50);

        // Step 7: Torque back on
        robot.TorqueON(BROADCAST);
        delay(50);
        currentMode = POSITION_MODE;
        if (Serial) Serial.printf("CALIB: done. raw1=%d raw2=%d offset1=%d offset2=%d\n", raw1, raw2, offset1, offset2);

        JsonDocument doc;
        doc["ok"] = true;
        doc["raw1"] = raw1;
        doc["raw2"] = raw2;
        doc["offset1"] = offset1;
        doc["offset2"] = offset2;
        doc["min"] = CALIB_MIN;
        doc["max"] = CALIB_MAX;
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
    });

    server.on("/api/calibrate/test", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        if (Serial) Serial.println("CALIB: test starting");
        robot.TorqueOFF(BROADCAST);
        delay(50);
        robot.setControlMode(BROADCAST, POSITION_MODE);
        delay(50);
        robot.TorqueON(BROADCAST);
        delay(50);
        currentMode = POSITION_MODE;
        // Center
        if (Serial) Serial.printf("CALIB: test -> center (%d)\n", CALIB_CENTER);
        robot.setJointPosition(MOTOR1, CALIB_CENTER);
        delay(5);
        robot.setJointPosition(MOTOR2, CALIB_CENTER);
        delay(2500);
        // Min
        if (Serial) Serial.printf("CALIB: test -> min (%d)\n", CALIB_MIN);
        robot.setJointPosition(MOTOR1, CALIB_MIN);
        delay(5);
        robot.setJointPosition(MOTOR2, CALIB_MIN);
        delay(2500);
        // Max
        if (Serial) Serial.printf("CALIB: test -> max (%d)\n", CALIB_MAX);
        robot.setJointPosition(MOTOR1, CALIB_MAX);
        delay(5);
        robot.setJointPosition(MOTOR2, CALIB_MAX);
        delay(2500);
        // Back to center
        if (Serial) Serial.printf("CALIB: test -> center\n");
        robot.setJointPosition(MOTOR1, CALIB_CENTER);
        delay(5);
        robot.setJointPosition(MOTOR2, CALIB_CENTER);
        delay(2000);
        if (Serial) Serial.println("CALIB: test complete");
        return response->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/calibrate/reset", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
        if (Serial) Serial.println("CALIB: resetting to defaults");
        robot.TorqueOFF(BROADCAST);
        delay(50);
        robot.setHomingOffset(MOTOR1, 0);
        delay(50);
        robot.setHomingOffset(MOTOR2, 0);
        delay(50);
        robot.setMinPositionLimit(MOTOR1, 0);
        delay(50);
        robot.setMaxPositionLimit(MOTOR1, 4095);
        delay(50);
        robot.setMinPositionLimit(MOTOR2, 0);
        delay(50);
        robot.setMaxPositionLimit(MOTOR2, 4095);
        delay(50);
        robot.TorqueON(BROADCAST);
        delay(50);
        currentMode = POSITION_MODE;
        if (Serial) Serial.println("CALIB: reset complete");
        return response->send(200, "application/json", "{\"ok\":true}");
    });

    server.on("/api/puppet/status", HTTP_GET, [](PsychicRequest *request, PsychicResponse *response) {
        JsonDocument doc;
        doc["state"] = puppetState == PUPPET_IDLE ? "idle" : puppetState == PUPPET_CONTROLLING ? "controller" : "puppet";
        doc["pairId"] = puppetPairId;
        String out;
        serializeJson(doc, out);
        return response->send(200, "application/json", out.c_str());
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

    // Stats every 5 seconds
    if (Serial && millis() - lastStatsTime > 5000 && wsFrameCount > 0) {
        Serial.printf("STATS t=%lu ws_frames=%u loop_proc=%u client_seq=%u (ws_recv=%.0f%% of sent)\n",
            millis(), wsFrameCount, loopProcessCount, lastSeqNum,
            lastSeqNum > 0 ? 100.0f * wsFrameCount / lastSeqNum : 0);
        lastStatsTime = millis();
    }

    // Handle motor commands on main thread
    if (newData) {
        newData = false;
        loopProcessCount++;
        if (Serial) Serial.printf("TILTY t=%lu b=%.2f g=%.2f -> m1=%d m2=%d\n", millis(), lastBVal, lastGVal, targetM1, targetM2);
        if (currentMode == DRIVE_MODE) {
            if (abs(targetM1 - prevM1) > 5 || targetM1 == 0) {
                robot.setJointSpeed(MOTOR1, targetM1);
                prevM1 = targetM1;
                delay(1);
            }
            if (abs(targetM2 - prevM2) > 5 || targetM2 == 0) {
                robot.setJointSpeed(MOTOR2, targetM2);
                prevM2 = targetM2;
                delay(1);
            }
        } else {
            if (abs(targetM1 - prevM1) > 10) {
                robot.setJointPosition(MOTOR1, targetM1);
                prevM1 = targetM1;
                delay(1);
            }
            if (abs(targetM2 - prevM2) > 10) {
                robot.setJointPosition(MOTOR2, targetM2);
                prevM2 = targetM2;
                delay(1);
            }
        }
    }

    // Controller mode: read positions and broadcast
    if (puppetState == PUPPET_CONTROLLING) {
        int32_t m1 = readMotorPosition(MOTOR1);
        int32_t m2 = readMotorPosition(MOTOR2);
        if (m1 >= 0 && m2 >= 0) {
            PuppetPacket pkt;
            pkt.pairId = puppetPairId;
            // Convert motor positions to degrees from center
            pkt.tilt = (m1 - CALIB_CENTER) * 0.088f;
            pkt.pan = (m2 - CALIB_CENTER) * 0.088f;
            uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_err_t result = esp_now_send(broadcastAddr, (uint8_t *)&pkt, sizeof(pkt));
            static unsigned long lastPuppetLog = 0;
            if (Serial && millis() - lastPuppetLog > 500) {
                Serial.printf("PUPPET TX: pair=%d tilt=%.1f pan=%.1f err=%d\n", pkt.pairId, pkt.tilt, pkt.pan, result);
                lastPuppetLog = millis();
            }
        } else {
            if (Serial) Serial.printf("PUPPET TX: read failed m1=%d m2=%d\n", m1, m2);
        }
        delay(10); // ~100Hz
        return;
    }

    // Puppet timeout: stop if no data for 2 seconds
    if (puppetState == PUPPET_FOLLOWING && millis() - lastPuppetRx > 2000) {
        // Still following but no data — just hold position
    }

    delay(1);
}
