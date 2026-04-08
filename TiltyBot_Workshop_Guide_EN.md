# TiltyBot Workshop Guide

A hands-on workshop for building and interacting with a smartphone-controlled robot using an ESP32-S3, Dynamixel motors, and open-source software.

---

## Table of Contents

1. [Preparation](#1-preparation)
2. [Software Setup — Building the Base Robot](#2-software-setup--building-the-base-robot)
3. [Hardware Assembly](#3-hardware-assembly)
4. [Calibration](#4-calibration)
5. [Puppet Mode](#5-puppet-mode)
6. [Interaction Design](#6-interaction-design)

---

## 1. Preparation

This section covers everything you need to do before the workshop begins. If you get stuck on any step, don't worry — these can also be completed in person at the start of the workshop.

### Before You Begin

- You will use a **personal smartphone** to control the robot. Corporate/managed phones may not work due to security restrictions.
- **Gyro tilty mode requires Android** (uses RelativeOrientationSensor). Manual sliders work on any device.

### Step 1: Install Tools

You can use VS Code with PlatformIO, or just the CLI.

**Option A: VS Code** (recommended for beginners)

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO](https://platformio.org/) extension — install from VS Code Extensions tab
- [Git](https://git-scm.com/download/win) (Windows) / Git is pre-installed on most Macs

**Option B: CLI only**

- [Git](https://git-scm.com/)
- PlatformIO Core:
  ```bash
  pip install platformio
  # or
  curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
  python3 get-platformio.py
  ```

### Step 2: Clone the TiltyBot Repository

Clone the project repository to a convenient location on your computer (e.g., your Desktop).

1. Open a terminal (or right-click your desired folder and select **Git Bash Here** on Windows).

<!-- TODO: update screenshot for new repo -->
![Opening Git Bash from a folder](img/GitBashHere.png)

2. Run the following command:

```
git clone https://github.com/imandel/tiltybot.git
```

### Step 3: Install PlatformIO (VS Code only)

If using VS Code:

1. Go to **View → Extensions**. Search for **PlatformIO** and install it.
2. Click the PlatformIO icon (ant icon) in the left sidebar to trigger initialization. Wait until it finishes.
3. Click **Reload Now** when prompted.

### Step 4: Build the Project

**VS Code:** Open the `tiltybot` folder in PlatformIO, then click the **Build button** (checkmark at the bottom).

**CLI:**

```bash
cd tiltybot
pio run -e tiltybot
```

Wait for "SUCCESS." Preparation is now complete!

---

## 2. Software Setup — Building the Base Robot

### Step 1: Configure Motors

Each Dynamixel motor needs a unique ID. We'll use a setup tool to assign them.

**Important:** Motor 1 = **tilt** (nod up/down), Motor 2 = **pan** (rotate left/right).

1. Upload the motor setup tool:

   ```bash
   pio run -e motor_setup -t upload
   ```

2. Open the Serial Monitor:

   **VS Code:** Click the plug icon at the bottom status bar.

   **CLI:**
   ```bash
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
If not press the reset button on the ESP32 while connected to the serial monitor.

3. With **one motor connected**, type `1` and press Enter. The motor's LED will blink and it will rotate back and forth to confirm.

4. Disconnect the first motor. Connect the **second motor**. Reset the board (press the reset button or unplug/replug USB). Type `2` and press Enter.

5. Daisy-chain both motors together (plug Motor 2 into Motor 1's other port, Motor 1 connects to the board). Reset the board. Type `t` to run the full test:
   - Both LEDs blink individually
   - Position mode: both motors move through several positions
   - Drive mode: both motors spin forward, backward, slow, and max speed

If the test passes, your motors are ready!

### Step 2: Set Wi-Fi Name

The robot hosts its own local Wi-Fi network (no internet required). You'll connect your phone to this network to control the robot.

Set the SSID at build time:

```bash
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-yourname\"' pio run -e tiltybot -t upload
```

Or edit `src/tiltybot/main.cpp` directly and change:

```c
#define BOT_SSID "YOURGROUPNAME"
```

The password defaults to `12345678` (must be 8+ characters).

### Step 3: Upload SSL Certificates and HTML Pages

The robot uses HTTPS so your phone's gyroscope can work in the browser. Upload the filesystem (SSL certificates + all HTML pages):

```bash
pio run -e tiltybot -t uploadfs
```

### Step 4: Upload Firmware

```bash
pio run -e tiltybot -t upload
```

Or with a custom SSID:

```bash
PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-yourname\"' pio run -e tiltybot -t upload
```

### Step 5: Connect Your Phone to the Robot

1. On your phone, go to Wi-Fi settings and connect to the network you configured in Step 2. Ignore any "No Internet" warnings.

2. Open a browser and navigate to `https://192.168.4.1`

3. You'll see a security warning about the self-signed certificate. Tap **Advanced** → **Proceed** (or equivalent for your browser).

4. The index page shows all available modes — pick one!

<!-- TODO: screenshot of index page on phone -->

**Tips:**
- Turn off cellular data on your phone to prevent it from automatically switching away from the robot's network.
- Disconnect from any VPN before connecting.

### Step 6: Test the 2-Motor Mode

Start with **2-Motor mode** to verify everything works. Move the sliders — both motors should respond.

**Important:** Before disconnecting the motors or assembling a robot, use 2-Motor mode to set both motor positions to **0** (or 180°). Do not manually rotate the motors by hand.

---

## 3. Hardware Assembly

Choose one of the two robot types to build:

| Driving Robot | Tilty Robot |
|---|---|
| Mobile robot on wheels | Dashboard robot that tilts and pans |

### Wiring

Connect the ESP32-S3-Zero to the Dynamixel motors:

| Dynamixel XL330 Pin | Signal | ESP32-S3-Zero |
|---|---|---|
| Pin 1 — GND | Ground | **GND** |
| Pin 2 — VDD | Power (5V) | **5V** |
| Pin 3 — Data | Half-duplex serial | **GPIO1 + GPIO2** (tied together) |

<!-- TODO: photo of wiring close-up -->

The two motors daisy-chain together — connect Motor 2 to the other port on Motor 1.

### Driving Robot

1. **Attach axles:** Screw the white axle pieces onto each motor using the provided screws.

![Motor with axle piece](img/IMG_7785.jpg)
![Axle screws](img/screws_axel.jpg)

2. **Attach wheels:** Insert the axles into the wheels.

![Wheel attached to axle](img/IMG_7817.jpg)

3. **Build a base:** Use cardboard (or similar material) to create a platform for mounting.

![Cardboard base](img/IMG_7818.jpg)

4. **Mount components:** Attach both Dynamixel motors and a roller ball to the base. Position them in a triangle for balance.

![Motors and roller ball mounted](img/IMG_7819.jpg)
![Completed drive robot base](img/IMG_7844.jpg)

5. **Connect and drive:** Join the robot's Wi-Fi, open `https://192.168.4.1`, tap **Drive Mode**, check **Active**, and use the joystick.

![Drive mode in action](img/drive_mode2.gif)

### Tilty Robot

**Prerequisite:** Before building the Tilty robot, open 2-Motor mode and set both motors to position **0**.

<!-- TODO: screenshot of 2-motor mode at 0 position -->

1. **Attach the head:** Screw the hinge bracket to **Motor 1** using the short screws. The "head" should face forward while at position 0.

![Head attached to Motor 1](img/IMG_7811.JPG)
![Bracket screws](img/screws_blaket.jpg)

2. **Attach the idler:** On **Motor 2**, attach only the idler and idler cap.

![Idler screws](img/screws_idler.jpg)

3. **Combine:** Attach the head/hinge assembly to Motor 2 in the correct orientation (the LED on top of Motor 2 should face you). **Do not rotate the motors manually!**

![Correct assembly orientation](img/IMG_7789.jpg)

4. **Connect and tilt:** Join the robot's Wi-Fi, open `https://192.168.4.1`, tap **Tilty Mode**. Use the sliders, or enable **Gyro** to control with your phone's orientation (Android only).

![Tilty mode in action](img/PanTilt_1.gif)

---

## 4. Calibration

After assembling the robot, calibrate so that the center position (2048) corresponds to "head level, looking straight ahead." Calibration writes to motor EEPROM and persists across power cycles.

### Why calibrate?

Every robot is assembled slightly differently. Without calibration, "center" might mean the head is tilted or rotated. Calibration also sets position limits to prevent the head from hitting the body.

### Procedure

1. Connect to the robot's Wi-Fi and open `https://192.168.4.1/calibrate.html`

2. **Release Motors** — click the button to turn off torque. You can now move the robot's head freely by hand.

3. **Position the robot** — physically move the head to the desired home position: level, facing straight ahead. Click **Read Positions** to see the raw motor values.

4. **Set Home** — click to write the homing offset to EEPROM. This makes the current position the new center (2048). Also sets default position limits (1024–3072, which is ±90° from center).

5. **Test Limits** — the robot moves to center, minimum, and maximum positions so you can verify the range looks correct.

6. If needed, **Reset** clears calibration back to factory defaults.

### Setting tilt limits for body clearance

The default ±90° tilt range may cause the head to collide with the robot's body. To find and set the actual safe range:

1. Release motors on the calibrate page
2. Gently move the tilt motor until it just touches the body in each direction
3. Read the positions at each extreme — note the values
4. These will be your tilt position limits

<!-- TODO: add UI for setting custom per-axis position limits -->

### Notes

- Always calibrate **after** assembly — the physical orientation depends on how the parts are mounted.
- Homing offset range is ±1024 (±90°). If the offset exceeds this, the motor silently ignores it.
- Both robots must be calibrated for puppet mode to work correctly — "center" must mean the same physical pose on both.

---

## 5. Puppet Mode

Puppet mode lets one robot mirror another's movements in real-time. Move the "controller" robot by hand, and the "puppet" robot follows.

### Requirements

- Two assembled and **calibrated** robots
- Both flashed with the same firmware (different SSIDs)
- Both powered on

### Setup

1. **Flash both robots** with different SSIDs so you can tell them apart:
   ```bash
   pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem1301
   PLATFORMIO_BUILD_FLAGS='-DBOT_SSID=\"BOT-red\"' pio run -e tiltybot -t upload --upload-port /dev/cu.usbmodem12301
   ```

2. **Calibrate both robots** (see [Calibration](#4-calibration) above). This is important — puppet mode sends angles in degrees, and each robot converts to its own motor positions.

3. **Set up the controller:**
   - Connect your phone to robot A's Wi-Fi
   - Open `https://192.168.4.1/puppet.html`
   - Pick an emoji (e.g., 🍻)
   - Tap **Controller**
   - The motors release — you can now move robot A by hand

4. **Set up the puppet:**
   - Connect your phone to robot B's Wi-Fi
   - Open `https://192.168.4.1/puppet.html`
   - Pick the **same emoji** (🍻)
   - Tap **Puppet**
   - Robot B starts following robot A's movements

5. **Stop:** tap the Stop button on either robot's puppet page.

### How it works

- The controller reads its motor positions, converts to degrees from center, and broadcasts via **ESP-NOW** (~100Hz)
- The puppet receives the angle data and converts to its own motor positions using its own calibration
- ESP-NOW is peer-to-peer wireless that works alongside WiFi with ~1-5ms latency
- Both APs must be on the same WiFi channel (both default to channel 1)
- The emoji is a simple pairing mechanism — only robots with the same emoji respond to each other

---

## 6. Interaction Design

Now that your robot is built, it's time to think about how people will interact with it. This section guides you through a design thinking exercise.

### Overview

A. [Plan](#part-a-plan)
B. [Act Out the Interaction](#part-b-act-out-the-interaction)
C. [Design the Appearance](#part-c-design-the-appearance)
D. [Control the Device](#part-d-control-the-device)
E. [Record](#part-e-record)

### Part A: Plan

Think about the interaction you want to create. Consider these four elements:

1. **Setting:** Where and when does this interaction take place? (e.g., a kitchen, a hospital, outdoors)
2. **Players:** Who is involved? Think about all the people present — the primary user, bystanders, children, roommates, etc. Poor designs often fail because they don't account for everyone in the environment.
3. **Activity:** What happens between the players and the robot?
4. **Goals:** What is each player trying to accomplish?

Write down your setting, players, activity, and goals.

### Part B: Act Out the Interaction

Physically act out the scenario you planned — without the robot. Have someone pretend to be the device and perform its scripted actions.

After acting it out, reflect:

- Did anything work better on paper than in practice?
- Did any new ideas or insights emerge from the performance?

### Part C: Design the Appearance

Now consider what the robot should look like. Think about:

- The environment: Could it overheat? Is water a risk? Does it need to be visible in emergencies?
- The users: What form factor would feel natural and inviting for the people who will interact with it?

Sketch your design on paper, then build a physical prototype using craft materials (cardboard, tape, markers, etc.).

### Part D: Control the Device

Practice controlling the robot so it performs the actions from your scenario. Have one person operate the controller while another interacts with the robot as a user.

### Part E: Record

Film a video of your prototyped interaction. Show the full scenario: the setting, the players, and the robot performing its role. This video captures the essence of your design idea and can be used for presentations or further iteration.

---

## Resources

- **TiltyBot Repository:** [https://github.com/imandel/tiltybot](https://github.com/imandel/tiltybot)
- **Visual Studio Code:** [https://code.visualstudio.com/](https://code.visualstudio.com/)
- **PlatformIO:** [https://platformio.org/](https://platformio.org/)
- **Git:** [https://git-scm.com/](https://git-scm.com/)
