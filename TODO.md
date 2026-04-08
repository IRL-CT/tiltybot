# TiltyBot — TODO

## Features

- [ ] **Captive portal** — Auto-open control page when joining WiFi (DNS redirect). Best workshop UX — no URL typing needed.
- [ ] **mDNS** — Advertise as `tiltybot.local` for easier access (works on Apple devices, spotty on Android).
- [ ] **Tilty gain multiplier** — Small phone movements should map to larger robot range. Start with 2x, tune to feel. Currently 1:1.
- [ ] **Re-center button** — Recapture gyro reference quaternion without toggling gyro off/on.
- [ ] **Custom tilt/pan position limits UI** — Calibrate page currently sets symmetric ±90°. Add per-axis limit setting based on body clearance measurement.
- [ ] **iOS tilty support** — Deferred. Would need DeviceOrientationEvent fallback with gimbal lock mitigation.

## Hardware

- [ ] **Wiring diagram** — Create a proper diagram showing ESP32-S3-Zero to Dynamixel connections.

## Documentation

- [ ] **Workshop guide screenshots** — Update images marked with `<!-- TODO -->`:
  - Screenshot of index page on phone
  - Photo of wiring close-up
  - Screenshot of 2-motor mode at 0 position
- [ ] **Japanese workshop guide** — Translate updates from EN guide to `TiltyBot_Workshop_Guide_JP.md`.

## Done

- [x] Move HTML to LittleFS — all pages served from `data/`
- [x] Tilty gyro mode — RelativeOrientationSensor with delta quaternion, zero gimbal lock
- [x] Puppet mode — ESP-NOW pairing with emoji grid, degree-based packets
- [x] Calibration page — homing offset + position limits via web UI
- [x] Sound page — TTS, synthesized soundboard, recording, file upload, saved sounds (IndexedDB)
- [x] Half-duplex echo fix — custom readMotorPosition() skips TX echo bytes
- [x] WiFi AP-only mode — eliminates STA scanning latency
- [x] BOT_SSID build flag — set SSID at compile time
