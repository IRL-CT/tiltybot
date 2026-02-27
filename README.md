# TiltyBot

A smartphone-controlled robot platform built on the ESP32 (TinyPICO) and Dynamixel XL330 servo motors. The robot hosts its own Wi-Fi network and serves a web interface that lets you control it from your phone's browser — no app install required.

This repo contains architectural improvments building on the incredible Rei Lee's [Conebot](https://github.com/rei039474/ConeBot)

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

The TinyPICO runs an HTTPS server over a local Wi-Fi access point. You connect your phone to that network, open the control page in a browser, and send commands to the motors over WebSocket. There are three control modes:

- **2motor** — Direct position control of two motors (0–360°). Useful as a starting point for custom builds.
- **tilty** — A pan/tilt head that responds to your phone's gyroscope orientation, or manual sliders.
- **drive** — Differential drive with a virtual joystick. Two wheels plus a caster ball.

There's also a sound recorder page that lets you capture and play back audio clips through a Bluetooth speaker connected to a second phone.

## Hardware

- [TinyPICO](https://www.tinypico.com/) (ESP32-based)
- 2x [Dynamixel XL330](https://robotis.us/dynamixel-xl330-m077-t/) servo motors
- [XL330 Hinge](https://www.robotis.us/fpx330-h101-4pcs-set/)
- USB-A to USB-C cable
- Battery
- Wheels
- Caster wheels
- [mini bluetooth speaker](https://a.co/d/07X8uOIN) 
- Cardboard, tape, and whatever else you want to build with

The motors daisy-chain together and connect to the TinyPICO via Serial2 (RX: GPIO 14, TX: GPIO 4).

## Software requirements

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO](https://platformio.org/) extension for VS Code
- [Git](https://git-scm.com/)
- macOS Sonoma+ (if on Mac)
- iOS 17.3.1+ or any modern Android browser (for the phone controller)

## Getting started

Clone the repo:

```
git clone https://github.com/imandel/tiltybot.git
cd tiltybot
```

Open the project in VS Code with PlatformIO installed.

### Configure your network

Edit `src/network.h` and set a unique SSID and password:

```c
const char *ssid = "my-robot";
const char *password = "something";
```

### Select a control mode

In `platformio.ini`, uncomment the mode you want under `build_src_filter` and comment out the others:

```ini
build_src_filter =
	-<*>
	+<init.cpp>
	+<init.h>
	; +<2motor.cpp>
	+<tilty.cpp>
	; +<drive.cpp>
```

### Upload

1. Connect the TinyPICO to your computer via USB.
2. Upload the firmware (arrow button at the bottom of VS Code, or PlatformIO: Upload).
3. Build the filesystem image: PlatformIO sidebar → Platform → Build Filesystem Image.
4. Upload the filesystem image: PlatformIO sidebar → Platform → Upload Filesystem Image. Make sure the Serial Monitor is closed before this step.

### Connect

1. Open the Serial Monitor to find the IP address.
2. On your phone, join the Wi-Fi network you configured.
3. Navigate to the URL shown in the monitor (e.g. `https://192.168.4.1/tilty.html`).
4. Ignore the self-signed certificate warning and proceed.

The URL path depends on the mode: `/2motor.html`, `/tilty.html`, or `/drive.html`. There's also an index page at `/` with links to all modes.

## Project structure

```
src/
  init.cpp / init.h   — Wi-Fi setup, HTTPS server, LittleFS, SSL cert generation, motor init
  network.h            — SSID and password config
  2motor.cpp           — 2-motor position control mode
  tilty.cpp            — Pan/tilt gyroscope mode
  drive.cpp            — Differential drive mode

data/
  *.html / *.js        — Browser-side control interfaces
  sound.html / sound.js — Audio recorder/player
  lib/                 — Pico CSS, joystick controller library
```

## Dependencies

Managed by PlatformIO (see `platformio.ini`):

- [ESP32_HTTPS_Server](https://github.com/khoih-prog/ESP32_HTTPS_Server.git)
- [Dynamixel_XL330_Servo_Library](https://github.com/rei039474/Dynamixel_XL330_Servo_Library.git)
- [ArduinoJson 5.13.4](https://github.com/bblanchon/ArduinoJson)

## Notes

- The robot generates a self-signed SSL certificate on first boot and stores it in LittleFS. First boot may take a minute.
- Turn off cellular data on your phone so it doesn't drop the robot's network.
- Disconnect from any VPN before connecting.
- If you're building the tilty robot, set both motors to position 0 in 2motor mode before assembling. Don't rotate the motors by hand after that.
- Corporate/managed phones may not work due to network restrictions.

