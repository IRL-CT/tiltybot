# TiltyBot

A smartphone-controlled robot platform built on the ESP32-S3 and Dynamixel XL330 servo motors. The robot hosts its own Wi-Fi network and serves a browser-based control interface — no app install required.

Building on Rei Lee's [ConeBot](https://github.com/rei039474/ConeBot).

<p>
  <img src="assets/ConeBot.gif" alt="ConeBot waddling" width="260">
  <img src="assets/PanTilt_1.gif" alt="Pan-tilt gyro control" width="260">
  <img src="assets/drive_mode2.gif" alt="Two-wheeled drive mode" width="260">
</p>

## Guides

- **[TiltyBot Intro](TiltyBot_Intro_EN.md)** — connect and use
- **[TiltyBot Assembly & Advanced](TiltyBot_Advanced_EN.md)** — build, flash, modify

## What it does

Connect your phone to the robot's Wi-Fi, open `https://192.168.4.1`, and pick a mode:

- **Drive** — joystick differential drive
- **Tilty** — phone gyro pan/tilt (Android) or manual sliders
- **2-Motor** — direct position control
- **Puppet** — one robot mirrors another via ESP-NOW
- **Sound** — TTS, soundboard, recording
- **Calibrate** — set motor home position and limits

## Hardware

- [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm)
- 2× [Dynamixel XL330-M077-T](https://robotis.us/dynamixel-xl330-m077-t/)
- USB-C cable and power source

## Quick start

```bash
git clone https://github.com/imandel/tiltybot.git
cd tiltybot
pio run -e tiltybot -t upload
pio run -e tiltybot -t uploadfs
```

Connect your phone to the `BOT-*` Wi-Fi (password: `12345678`), open `https://192.168.4.1`.

## License

MIT
