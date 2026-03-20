# TiltyBot

A smartphone-controlled robot platform built on the ESP32-S3 and Dynamixel XL330 servo motors. The robot hosts its own Wi-Fi network and serves a web interface that lets you control it from your phone's browser — no app install required.

This repo contains architectural improvements building on the incredible Rei Lee's [Conebot](https://github.com/rei039474/ConeBot)

<figure>
  <img src="img/ConeBot.gif" alt="A small cone robot waddling" width="300">
  <figcaption>Rei's Original ConeBot</figcaption>
</figure>

<figure>
  <img src="img/PanTilt_1.gif" alt="A small pan tilt robot head" width="300">
  <figcaption>A tiltyBot in gyro control mode</figcaption>
</figure>

<figure>
  <img src="img/drive_mode2.gif" alt="A small two wheeled robot made of cardboard" width="300">
  <figcaption>A tiltyBot in drive control mode</figcaption>
</figure>

## What it does

The ESP32-S3 runs an HTTPS server over a local Wi-Fi access point. You connect your phone to that network, open the control page in a browser, and send commands to the motors over WebSocket. All three control modes are available from a single firmware — pick from the index page:

- **Drive** — Differential drive with a virtual joystick. Two wheels plus a caster ball.
- **Tilty** — A pan/tilt head that responds to your phone's gyroscope orientation, or manual sliders.
- **2-Motor** — Direct position control of two motors (0–360°). Useful as a starting point for custom builds.

## Hardware

- [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm) (ESP32-S3, 4MB Flash, 2MB PSRAM)
- 2× [Dynamixel XL330](https://robotis.us/dynamixel-xl330-m077-t/) servo motors
- [XL330 Hinge](https://www.robotis.us/fpx330-h101-4pcs-set/)
- USB-C cable
- Wheels, caster wheels
- Cardboard, tape, and whatever else you want to build with

### Wiring

The motors daisy-chain together and connect to the ESP32-S3-Zero via 3 wires:

| Dynamixel XL330 Pin | Signal | ESP32-S3-Zero |
|---|---|---|
| Pin 1 — GND | Ground | **GND** |
| Pin 2 — VDD | Power (5V) | **5V** |
| Pin 3 — Data | Half-duplex serial | **GPIO1 + GPIO2** (tied together) |

GPIO1 (TX) and GPIO2 (RX) are next to each other on the left side of the board. Both connect to the single Dynamixel Data wire for half-duplex communication.

## Software requirements

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO](https://platformio.org/) extension for VS Code
- [Git](https://git-scm.com/)

## Getting started

Clone the repo:

```
git clone https://github.com/imandel/tiltybot.git
cd tiltybot
```

Open the project in VS Code with PlatformIO installed.

### 1. Configure motors

Each motor needs a unique ID. Flash the motor setup tool:

```
pio run -e motor_setup -t upload
```

Open the Serial Monitor (115200 baud). Connect **one motor** at a time:

1. Connect Motor 1, type `1` → configures it as ID 1 at 115200 baud
2. Disconnect, connect Motor 2, reset the board, type `2` → configures it as ID 2
3. Daisy-chain both motors, reset the board, type `t` → runs a full test

### 2. Configure your network

Edit the WiFi credentials near the top of `src/tiltybot/main.cpp`:

```c
const char *ssid = "my-robot";
const char *password = "something";  // must be 8+ characters
```

### 3. Upload

Upload the SSL certificates (first time only, or after changing certs):

```
pio run -e tiltybot -t uploadfs
```

Upload the firmware:

```
pio run -e tiltybot -t upload
```

### 4. Connect

1. On your phone, join the Wi-Fi network you configured.
2. Open `https://192.168.4.1` in your browser.
3. Accept the self-signed certificate warning.
4. Pick a control mode from the menu.

## Project structure

```
src/
  tiltybot/main.cpp      — Unified firmware (all three modes)
  motor_setup/main.cpp   — Motor configuration tool

data/
  server.crt / server.key — Self-signed SSL certificate

partitions.csv            — Custom partition table (4MB flash)
platformio.ini            — PlatformIO configuration
```

## PlatformIO environments

| Environment | Description |
|---|---|
| `tiltybot` | Main firmware — all control modes |
| `motor_setup` | Motor ID/baud configuration tool |

Build and upload:

```bash
pio run -e tiltybot -t upload      # flash firmware
pio run -e tiltybot -t uploadfs    # flash SSL certs to LittleFS
pio run -e motor_setup -t upload   # flash motor setup tool
```

## Dependencies

Managed by PlatformIO (see `platformio.ini`):

- [PsychicHttp](https://github.com/hoeken/PsychicHttp) — HTTPS server with WebSocket support
- [Dynamixel_XL330_Servo_Library](https://github.com/rei039474/Dynamixel_XL330_Servo_Library.git) — Motor control
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — JSON parsing

Platform: [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino Core 3.x / ESP-IDF 5.x)

## Notes

- **Password must be 8+ characters** — shorter passwords will prevent devices from joining the WiFi network.
- **HTTPS is required** for gyroscope access — browsers only allow `DeviceOrientationEvent` in secure contexts.
- Turn off cellular data on your phone so it doesn't drop the robot's network.
- Disconnect from any VPN before connecting.
- If you're building the tilty robot, set both motors to position 0 in 2-motor mode before assembling. Don't rotate the motors by hand after that.
- Corporate/managed phones may not work due to network restrictions.
- The SSL certificate in `data/` is self-signed. To regenerate:
  ```
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
    -keyout data/server.key -out data/server.crt -days 3650 -nodes \
    -subj "/CN=tiltybot.local"
  ```
