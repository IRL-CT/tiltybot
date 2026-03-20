# TiltyBot — TODO

## Features

- [ ] **Captive portal** — Auto-open control page when joining WiFi (DNS redirect). Best workshop UX — no URL typing needed.
- [ ] **mDNS** — Advertise as `tiltybot.local` for easier access (works on Apple devices, spotty on Android).
- [ ] **Sound page improvements** — Currently client-side only. Consider adding BT speaker pairing instructions to the UI.

## Hardware

- [ ] **Test gyroscope** — Verify tilty mode with phone gyroscope over HTTPS (DeviceOrientationEvent).

## Documentation

- [ ] **Workshop guide screenshots** — Update images marked with `<!-- TODO -->` in `TiltyBot_Workshop_Guide_EN.md`:
  - Photo of ESP32-S3-Zero wired to motors
  - Screenshot of index page on phone
  - Photo of wiring close-up
  - Screenshot of 2-motor mode at 0 position
- [ ] **Japanese workshop guide** — Translate updates from EN guide to `TiltyBot_Workshop_Guide_JP.md`.
- [ ] **Wiring diagram** — Create a proper diagram showing ESP32-S3-Zero ↔ Dynamixel connections.

## Code Quality

- [ ] **Move HTML to LittleFS** — Serve pages from `data/` as standard `.html` files instead of inline PROGMEM strings. Easier to edit, preview, and customize without recompiling. Upload with `pio run -e tiltybot -t uploadfs`.
