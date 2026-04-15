# TiltyBot Assembly & Advanced

## Hardware

- [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm) (ESP32-S3, 4MB Flash, 2MB PSRAM)
- 2× [Dynamixel XL330-M077-T](https://robotis.us/dynamixel-xl330-m077-t/) servo motors
- [XL330 Hinge Frame](https://www.robotis.us/fpx330-h101-4pcs-set/)
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

## Software Setup

Install:

- [Git](https://git-scm.com/)
- [PlatformIO](https://platformio.org/) — either as a VS Code extension or CLI-only

Clone the repository:

```bash
git clone https://github.com/imandel/tiltybot.git
cd tiltybot
```

Verify the build:

```bash
pio run -e tiltybot
```

## Motor Setup

Each motor needs a unique ID. Flash the motor setup tool:

```bash
pio run -e motor_setup -t upload
pio device monitor -b 115200
```

Connect **one motor** at a time:

1. Connect Motor 1, type `1` → configures as ID 1 (LED blinks, motor moves to confirm)
2. Disconnect Motor 1, connect Motor 2, reset board, type `2` → configures as ID 2
3. Daisy-chain both motors, reset board, type `t` → runs full test

**Motor 1 = tilt** (nod up/down), **Motor 2 = pan** (rotate left/right).

## Build / Flash / UploadFS

Upload firmware:

```bash
pio run -e tiltybot -t upload
```

Upload web UI and certificates to the filesystem:

```bash
pio run -e tiltybot -t uploadfs
```

Both steps are needed when setting up a new robot. After that, you only need to re-upload whichever part you changed.

**If the ESP32-S3 won't enter download mode** (upload fails with "No serial data received"):

1. Hold **BOOT**
2. Tap **RESET**
3. Release **BOOT**
4. Retry the upload command

## Configuration (`BOT_SSID`)

Each robot's Wi-Fi network name is set at build time:

```bash
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID="BOT-yourname"' pio run -e tiltybot -t upload
```

The default (if no flag is set) is defined in `main.cpp`:

```c
#define BOT_SSID "YOURGROUPNAME"
```

The Wi-Fi password is `12345678`.

## Project Layout

```
tiltybot/
├── src/
│   ├── tiltybot/main.cpp      # Main firmware (all control modes)
│   └── motor_setup/main.cpp   # Motor ID assignment utility
├── data/                      # Web UI (served from LittleFS)
│   ├── style.css              # Shared styles
│   ├── index.html             # Landing page
│   ├── drive.html             # Drive mode
│   ├── tilty.html             # Tilty mode
│   ├── 2motor.html            # 2-Motor mode
│   ├── puppet.html            # Puppet mode
│   ├── sound.html             # Sound mode
│   └── calibrate.html         # Calibration
├── certs/                     # Self-signed SSL certificates
├── partitions.csv             # Custom partition table (4MB flash)
└── platformio.ini             # Build configuration
```

## System Architecture

- The ESP32 runs as a **Wi-Fi access point** and HTTPS server
- Web pages are stored on **LittleFS** and served to the browser
- Browser control pages communicate with the firmware over **WebSocket**
- Each mode has its own WebSocket endpoint (`/ws/drive`, `/ws/tilty`, `/ws/2motor`)
- Drive mode uses **Dynamixel Sync Write** to update both motors in a single bus packet
- Tilty mode uses `RelativeOrientationSensor` (Android) with delta quaternion decomposition for gimbal-lock-free pan/tilt control
- Puppet mode uses **ESP-NOW** for direct robot-to-robot communication (~1–5ms latency, ~100Hz). Emoji selection acts as a pairing filter. Packets carry angles in degrees; each robot maps through its own calibration

## Debugging / Logs

```bash
pio device monitor -b 115200
```

Key log lines:

| Log prefix | Meaning |
|---|---|
| `Motor mode -> DRIVE/POSITION` | Mode switch on WebSocket connect |
| `DRIVE moving: ...` | Joystick input started |
| `DRIVE -> stop: ...` | Joystick released, writing zeros |
| `DRIVE watchdog: ...` | No client frame for 300ms, emergency stop |
| `DRIVE X.XHz ...` | Periodic drive status (every 2s) |
| `TILTY ...` | Tilty/2-motor position update |
| `PUPPET TX: ...` | Controller broadcasting position |
| `ESP-NOW rx ...` | Puppet receiving data |
| `CALIB: ...` | Calibration operations |

If behavior seems wrong after physical changes, recalibrate before deeper debugging.

## CLI Reference

```bash
# Build
pio run -e tiltybot
pio run -e motor_setup

# Upload
pio run -e tiltybot -t upload
pio run -e tiltybot -t uploadfs
pio run -e motor_setup -t upload

# Custom SSID
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID="BOT-red"' pio run -e tiltybot -t upload

# Specify port
pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem101

# Monitor
pio device monitor -b 115200

# Clean / erase
pio run -e tiltybot -t clean
pio run -e tiltybot -t erase
```

## Dependencies

Managed automatically by PlatformIO (see `platformio.ini`):

- [PsychicHttp](https://github.com/hoeken/PsychicHttp) — HTTPS server with WebSocket support
- [Dynamixel_XL330_Servo_Library](https://github.com/rei039474/Dynamixel_XL330_Servo_Library.git) — Motor control
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — JSON parsing

## Notes

- HTTPS is required for gyroscope access — browsers only allow sensor APIs in secure contexts
- Turn off cellular data on your phone so it doesn't drop the robot's network
- Corporate/managed phones may not work due to network restrictions
- To regenerate the SSL certificate:
  ```bash
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 \
    -keyout data/server.key -out data/server.crt -days 3650 -nodes \
    -subj "/CN=tiltybot.local"
  ```
