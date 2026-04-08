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

The ESP32-S3 runs an HTTPS server over a local Wi-Fi access point. You connect your phone to that network, open the control page in a browser, and send commands to the motors over WebSocket. All control modes are available from a single firmware — pick from the index page:

- **Drive** — Differential drive with a virtual joystick. Two wheels plus a caster ball.
- **Tilty** — A pan/tilt head that responds to your phone's gyroscope orientation, or manual sliders. Gyro mode requires **Android** (uses RelativeOrientationSensor for clean quaternion input).
- **2-Motor** — Direct position control of two motors (0–360°). Useful as a starting point for custom builds.
- **Puppet** — One robot moved by hand controls another robot's servos in real-time via ESP-NOW. Both robots run the same firmware — pick controller or puppet role from the web UI.
- **Sound** — Record, upload, and play audio clips.
- **Calibrate** — Set homing offset and position limits via EEPROM. Persists across power cycles.

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

You can use either VS Code with PlatformIO, or the PlatformIO CLI directly.

### Option A: VS Code

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO](https://platformio.org/) extension for VS Code

### Option B: CLI only

Install PlatformIO Core:

```bash
# macOS / Linux
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py

# Or via pip
pip install platformio
```

Verify the installation:

```bash
pio --version
```

You'll also need [Git](https://git-scm.com/).

## Getting started

### 1. Clone the repo

```bash
git clone https://github.com/imandel/tiltybot.git
cd tiltybot
```

### 2. Configure motors

Each motor needs a unique ID. Flash the motor setup tool and open a serial connection:

```bash
pio run -e motor_setup -t upload
pio device monitor -b 115200
```

You'll see:

```
========================================
  TILTYBOT MOTOR SETUP
========================================

Options:
  1 - Configure this motor as Motor 1
  2 - Configure this motor as Motor 2
  t - Test both motors (daisy-chained)
```

Connect **one motor** at a time:

1. Connect Motor 1, type `1` → configures it as ID 1 at 115200 baud (LED blinks, motor moves to confirm)
2. Disconnect Motor 1, connect Motor 2, reset the board, type `2` → configures it as ID 2
3. Daisy-chain both motors, reset the board, type `t` → runs full test (LEDs, position mode, drive mode)

**Motor 1 = tilt** (nod up/down), **Motor 2 = pan** (rotate left/right).

### 3. Set the Wi-Fi name

The SSID defaults to `YOURGROUPNAME`. Set it at build time with a flag:

```bash
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-red\"' pio run -e tiltybot -t upload
```

Or edit `src/tiltybot/main.cpp` directly:

```c
#ifndef BOT_SSID
#define BOT_SSID "YOURGROUPNAME"
#endif
```

The password defaults to `12345678` (must be 8+ characters).

### 4. Upload

Upload the filesystem (HTML pages, SSL certs — first time, or after editing HTML):

```bash
pio run -e tiltybot -t uploadfs
```

Upload the firmware:

```bash
pio run -e tiltybot -t upload
```

### 5. Connect

1. On your phone, join the Wi-Fi network you configured.
2. Open `https://192.168.4.1` in your browser.
3. Accept the self-signed certificate warning.
4. Pick a control mode from the menu.

## Calibration

After assembling the robot, calibrate so that position 2048 = center (head level, looking straight ahead). This writes homing offset and position limits to motor EEPROM — persists across power cycles.

### Via web UI

1. Connect to the robot's Wi-Fi and open `https://192.168.4.1/calibrate.html`
2. **Release Motors** — turns off torque so you can move the robot by hand
3. **Position the robot** — move the head to center (level, facing forward), click **Read Positions**
4. **Set Home** — writes homing offset so current position becomes 2048
5. **Test Limits** — moves to center, min, max to verify
6. **Reset** — clears calibration back to factory defaults if needed

### Position limits

The calibration page sets symmetric limits at 1024 and 3072 (±90° from center). For the tilty robot, the physical tilt limit is smaller due to body collision. Measure the actual limits using the calibrate page and update the motor EEPROM position limits accordingly.

## Puppet mode

Two robots paired so one mirrors the other's movements in real-time.

### How it works

- Both robots run the **same firmware**
- Communication is via **ESP-NOW** (peer-to-peer, ~1-5ms latency, works alongside WiFi)
- Pairing uses a shared emoji — both robots must select the same one
- The controller sends motor angles (in degrees from center), the puppet converts to its own motor positions using its own calibration

### Setup

1. Flash both robots with the same firmware (different SSIDs so you can tell them apart):
   ```bash
   pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem1301
   PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-red\"' pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem12301
   ```
2. **Calibrate both robots** — important so "center" means the same physical pose on both
3. Connect phone to robot A's WiFi, open puppet page, pick an emoji, tap **Controller**
4. Connect phone to robot B's WiFi, open puppet page, pick the **same emoji**, tap **Puppet**
5. Move robot A by hand — robot B follows

The controller turns off motor torque so you can move it freely. The puppet holds torque and tracks the controller's position. Both APs must be on the same WiFi channel (default: channel 1).

## Tilty mode — gyro control

The gyro mode uses Android's `RelativeOrientationSensor` to get clean hardware quaternions from the phone's IMU. This avoids gimbal lock and Euler angle artifacts that plague `DeviceOrientationEvent`.

**Android only** — iOS does not support `RelativeOrientationSensor`. Manual sliders work on any device.

### How it works

1. When you enable gyro, the phone captures a **reference quaternion** (your current hand position)
2. All subsequent readings are decomposed as a **delta** from that reference
3. YXZ Euler extraction gives decoupled tilt (±90°) and pan (±180°)
4. Singularity is at tilt = ±90° — unreachable during normal use since you'd need to point the phone straight up/down from your starting position

### Tips

- Hold the phone comfortably, then check the gyro box — that position becomes center
- Nod the phone for tilt, rotate for pan
- If it drifts, uncheck and recheck the gyro box to recapture the reference

## CLI reference

All commands are run from the project root.

### Build

```bash
pio run -e tiltybot                  # build firmware
pio run -e motor_setup               # build motor setup tool
```

### Upload

```bash
pio run -e tiltybot -t upload        # flash firmware
pio run -e tiltybot -t uploadfs      # flash HTML pages + SSL certs to LittleFS
pio run -e motor_setup -t upload     # flash motor setup tool
```

Set a custom SSID:

```bash
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-red\"' pio run -e tiltybot -t upload
```

If the port isn't auto-detected, specify it:

```bash
pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem101
```

### Serial monitor

```bash
pio device monitor -b 115200
```

Or find the port and use any serial tool:

```bash
pio device list                      # list available ports
screen /dev/cu.usbmodem101 115200    # macOS/Linux
```

### Clean

```bash
pio run -e tiltybot -t clean         # remove build artifacts
```

### Erase flash

If the board gets into a bad state:

```bash
pio run -e tiltybot -t erase         # erase entire flash
```

You'll need to re-upload both firmware and LittleFS after erasing.

## Project structure

```
src/
  tiltybot/main.cpp      — Main firmware (all control modes)
  motor_setup/main.cpp   — Motor ID/baud configuration tool

data/
  index.html             — Mode selection menu
  drive.html             — Differential drive with joystick
  tilty.html             — Gyro/slider pan-tilt control
  2motor.html            — Direct position control
  puppet.html            — Puppet pairing UI
  calibrate.html         — Homing offset + position limits
  sound.html             — Audio recorder/player
  server.crt / server.key — Self-signed SSL certificate

scripts/
  analyze_tilty.py       — Parse and summarize tilty serial logs
  plot_tilty.py          — Plot motor positions and sent values

partitions.csv           — Custom partition table (4MB flash)
platformio.ini           — PlatformIO configuration
```

## PlatformIO environments

| Environment | Description |
|---|---|
| `tiltybot` | Main firmware — all control modes |
| `motor_setup` | Motor ID/baud configuration tool |

## Dependencies

Managed automatically by PlatformIO (see `platformio.ini`):

- [PsychicHttp](https://github.com/hoeken/PsychicHttp) — HTTPS server with WebSocket support
- [Dynamixel_XL330_Servo_Library](https://github.com/rei039474/Dynamixel_XL330_Servo_Library.git) — Motor control (with homing offset + position limit support)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — JSON parsing

Platform: [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino Core 3.x / ESP-IDF 5.x)

## Notes

- **Password must be 8+ characters** — shorter passwords will prevent devices from joining the WiFi network.
- **HTTPS is required** for gyroscope access — browsers only allow sensor APIs in secure contexts.
- **Gyro tilty mode is Android-only** — uses `RelativeOrientationSensor`. Manual sliders work on any device.
- Turn off cellular data on your phone so it doesn't drop the robot's network.
- Disconnect from any VPN before connecting.
- If you're building the tilty robot, set both motors to position 0 in 2-motor mode before assembling. Don't rotate the motors by hand after that.
- Corporate/managed phones may not work due to network restrictions.
- The SSL certificate in `data/` is self-signed. To regenerate:
  ```bash
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
    -keyout data/server.key -out data/server.crt -days 3650 -nodes \
    -subj "/CN=tiltybot.local"
  ```
