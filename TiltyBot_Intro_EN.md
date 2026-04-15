# TiltyBot Intro

## [TiltyBot Advanced](TiltyBot_Advanced_EN.md)

## What TiltyBot Is

TiltyBot is a small robot built from an ESP32-S3 microcontroller and two Dynamixel XL330 servo motors. One motor controls **tilt** (nodding up and down) and the other controls **pan** (rotating left and right).

The robot runs its own Wi-Fi network and serves a browser-based control interface. You control it from your phone — no app install, no internet connection, no coding required.

![TiltyBot overview](assets/tiltybot_overview.jpg)

Example motion demos:

![ConeBot demo](assets/ConeBot.gif)

## How to Connect

1. On your phone, open Wi-Fi settings and look for a network starting with `BOT-` (e.g., `BOT-red`, `BOT-blue`)

2. Connect to it. The password is `12345678`. Your phone will say it can't provide internet — this is expected.

![Wi-Fi settings showing BOT-red](assets/wifi_connect.png)

3. Open a browser and go to `https://192.168.4.1`

4. You will see a certificate warning. Tap **Advanced**, then **Proceed to 192.168.4.1 (unsafe)**. The robot uses a self-signed certificate so the browser can access phone sensors like the gyroscope.

![Certificate warning](assets/cert_warning.png)

![Tap Advanced, then Proceed](assets/cert_proceed.png)

5. You should see the TiltyBot main menu:

![TiltyBot index page](assets/index_page.png)

## Modes

### Drive

Differential drive control using an on-screen joystick. Touch and drag to steer. The robot stops when you release.

![Drive mode](assets/drive_mode.png)

![Drive demo](assets/drive_mode2.gif)

### Tilty (Android only)

Controls the robot's pan and tilt using your phone's orientation. Hold your phone and tilt it — the robot follows.

> Gyro control requires an Android device. Manual sliders are available on any device as a fallback.

**Before using Tilty mode, calibrate the robot.**

Open the Calibrate page from the main menu:

![Calibrate page](assets/calibrate_page.png)

1. Tap **Release Motors** — this turns off motor torque so you can move the head by hand
2. Physically position the robot's head to the desired neutral pose: level, facing straight ahead
3. Tap **Read Positions** to confirm the motor values
4. Tap **Set Home** — this writes the current position as the new center to motor memory
5. Tap **Test Limits** — the robot moves through its range so you can verify it looks correct

Calibration persists across power cycles. You only need to redo it if you physically reassemble the robot.

Once calibrated, open Tilty mode. Use the sliders for manual control:

![Tilty mode with sliders](assets/tilty_sliders.png)

Or enable the **Gyro** checkbox to control with phone orientation:

![Tilty mode with gyro enabled](assets/tilty_gyro.png)

![Tilty demo](assets/PanTilt_1.gif)

### 2-Motor

Direct slider control of both motors independently. Useful for testing motor response or exploring range of motion.

![2-Motor mode](assets/2motor_mode.png)

### Puppet (two bots)

One robot mirrors another's movements in real time. You move the controller robot by hand, and the puppet robot follows.

**Setup:**

1. Open the Puppet page on both robots (from two phones, each connected to its own bot's Wi-Fi)
2. Pick the **same emoji** on both robots

![Puppet emoji selection](assets/puppet_emoji.png)

3. On one robot, tap **Controller** — its motors release so you can move it by hand
4. On the other robot, tap **Puppet** — it begins following the controller's movements
5. Tap **Stop** on either robot to end

For best results, calibrate both robots before using puppet mode.

### Sound

Text-to-speech, a synthesized soundboard, audio recording, and file playback.

Sound mode is designed for a **second operator**. One person controls the robot's movement (via Drive, Tilty, or Puppet) while another person operates Sound mode from a second phone connected to the same robot.

The first time you open Sound, your browser will ask for microphone permission. Tap **Allow while visiting the site** if you want to use the recorder.

![Microphone permission](assets/sound_mic_permission.png)

The Sound page has three sections: Speech (text-to-speech), Soundboard (synthesized effects), and Record (microphone recording and file upload). Sounds you save appear in the **Saved** section at the top for quick access.

![Sound page with saved sounds and all sections](assets/sound_full.png)

![Sound page showing collapsed sections](assets/sound_sections.png)

## If Something Doesn't Work

| Problem | Try |
|---|---|
| Can't find the Wi-Fi network | Make sure you're close to the robot and it's powered on |
| Page won't load | Confirm you're on the `BOT-*` network, go to `https://192.168.4.1` |
| Certificate error won't go away | Try a different browser (Chrome works best) |
| Motors don't respond | Reload the page, or try 2-Motor mode to check basic connectivity |
| Tilty gyro doesn't work | Confirm you're on an Android device. Toggle the Gyro checkbox off and on |
| Puppet doesn't follow | Check both robots selected the same emoji. Recalibrate both |
| Robot behaves strangely after reassembly | Recalibrate |
