# TiltyBot Advanced


This section is for participants with programming experience who want to understand how TiltyBot works under the hood, build additional extensions, modify its behavior, or reflash the firmware.

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

If this completes with `SUCCESS`, your toolchain is ready.

## Project Layout

```
tiltybot/
├── src/
│   ├── tiltybot/main.cpp      # Main firmware
│   └── motor_setup/main.cpp   # Motor ID/BAUD assignment utility
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
└── platformio.ini             # Build configuration
```

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

## System Architecture

- The ESP32 runs as a **Wi-Fi access point** and HTTPS server
- Web pages are stored on **LittleFS** and served to the browser
- Browser control pages communicate with the firmware over **WebSocket**
- Each mode has its own WebSocket endpoint (`/ws/drive`, `/ws/tilty`, `/ws/2motor`)
- Drive mode uses **Dynamixel Sync Write** to update both motors in a single bus packet
- Tilty mode uses `RelativeOrientationSensor` (Android) with delta quaternion decomposition for gimbal-lock-free pan/tilt control
- Puppet mode uses **ESP-NOW** for direct robot-to-robot communication (~1–5ms latency, ~100Hz). Emoji selection acts as a pairing filter. Packets carry angles in degrees; each robot maps through its own calibration

## Debugging / Logs

Open a serial monitor:

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

