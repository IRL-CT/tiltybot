# TiltyBot Workshop Guide

A hands-on workshop for building and interacting with a smartphone-controlled robot using TinyPICO, Dynamixel motors, and open-source software.

---

## Table of Contents

1. [Preparation](#1-preparation)
2. [Software Setup — Building the Base Robot](#2-software-setup--building-the-base-robot)
3. [Hardware Assembly](#3-hardware-assembly)
4. [Interaction Design](#4-interaction-design)

---

## 1. Preparation

This section covers everything you need to do before the workshop begins. If you get stuck on any step, don't worry — these can also be completed in person at the start of the workshop.

### Before You Begin

- **Mac users:** Make sure you are running **macOS Sonoma or later**.
- **iPhone users:** Make sure you are running **iOS 17.3.1 or later**.
- You will use a **personal smartphone** to control the robot. Corporate/managed phones may not work due to security restrictions.

### Step 1: Install Visual Studio Code and Git

Download and install both tools using the default settings:

- [Visual Studio Code](https://code.visualstudio.com/)
- [Git](https://git-scm.com/download/win) (Windows) / Git is pre-installed on most Macs

### Step 2: Clone the TiltyBot Repository

Clone the project repository to a convenient location on your computer (e.g., your Desktop).

1. Open a terminal (or right-click your desired folder and select **Git Bash Here** on Windows).

![Opening Git Bash from a folder](img/GitBashHere.png)

2. Run the following command:

```
git clone https://github.com/imandel/tiltybot.git
```

![Cloning the repository](img/GitClone.png)

### Step 3: Install PlatformIO in Visual Studio Code

1. In VS Code, go to **View → Extensions**. Search for **PlatformIO** and install it.

![Installing PlatformIO extension](img/image11.png)

2. Click the PlatformIO icon (ant icon) ![ant icon](img/image5.png) in the left sidebar to trigger initialization. Wait until the bottom-right corner shows "Finished."

![PlatformIO initializing](img/image14.png)

3. Click **Reload Now** when prompted.

### Step 4: Build the Project

1. Open the TiltyBot repository in PlatformIO: click the ant icon → **Quick Access → Open**, then select the cloned `tiltybot` folder.

![Opening the project in PlatformIO](img/image12.png)

2. In the file explorer (top icon in the left sidebar), open `platformio.ini`.

![Selecting platformio.ini](img/image10.png)

3. Click the **Build button** (checkmark at the bottom of the screen) and wait for it to complete.

![Build button location](img/image15.png)

Preparation is now complete!

---

## 2. Software Setup — Building the Base Robot

The base robot supports three control modes:

- **Drive mode:** A small mobile robot that drives around on wheels.
- **Tilty mode:** A dashboard-style robot that can tilt and look around.
- **2-Motor mode:** Direct control of two motors (0–360°), useful for designing your own robot.

### Before You Start

- Confirm your OS/iOS versions meet the requirements listed in the Preparation section.
- Connect the TinyPICO and Dynamixel motors to your laptop via USB.

![TinyPICO and motors connected to laptop](img/IMG_7822.jpg)

### Step 1: Select a Control Mode

Open `platformio.ini` in VS Code. Around line 28, you'll see three source file options:

```
+<2motor.cpp>
+<tilty.cpp>
+<drive.cpp>
```

Uncomment the mode you want to use and comment out the other two by adding `;` at the beginning of the line. For example, to enable 2-motor mode:

```
+<2motor.cpp>
;+<tilty.cpp>
;+<drive.cpp>
```

![Selecting a mode in platformio.ini](img/SelectingMode.png)

For the rest of this guide, we'll use **2-motor mode**. You can switch modes later by editing this file and re-uploading.

### Step 2: Set Wi-Fi Credentials

The robot hosts its own local Wi-Fi network (no internet). You'll connect your phone to this network to control the robot via a web browser.

Open `network.h` and set a unique SSID (network name) and password. Use something identifiable like your name or team name so it doesn't conflict with others.

![Editing network.h](img/networkh.png)

### Step 3: Upload Firmware to TinyPICO

Click the **Upload button** (→ arrow at the bottom of the screen) to upload the C++ firmware to the TinyPICO. This firmware handles Wi-Fi hosting, the web server, and communication between the robot and your browser.

![Upload button](img/Upload.png)

Wait for "SUCCESS" to appear:

![Successful upload](img/image7.png)

### Step 4: Build the Filesystem Image

The browser interface (HTML, JavaScript, CSS) needs to be uploaded separately to flash memory.

Click the **PlatformIO ant icon → Platform → Build Filesystem Image**.

![Build Filesystem Image menu](img/SerialMonitor.png)

Wait for "SUCCESS":

![Successful build](img/image7.png)

### Step 5: Upload the Filesystem Image

Click the **PlatformIO ant icon → Platform → Upload Filesystem Image** (not OTA).

![Upload Filesystem Image menu](img/UploadfilesystemImage.png)

**Important:** Make sure the Serial Monitor is closed before uploading. If it's open, hover over the Monitor tab at the bottom and click the trash icon to close it.

![Close the Serial Monitor before uploading](img/image9.png)

### Step 6: Connect Your Phone to the Robot

1. Open the **Serial Monitor** (plug icon at the bottom of VS Code) to see the robot's IP address. If nothing appears, press the **reset button** on the TinyPICO.

![Serial Monitor showing IP address](img/image18.png)

The reset button is on the surface of the device:

![Reset button location](img/IMG_7790.jpg)

2. On your phone, go to Wi-Fi settings and connect to the network you configured in Step 2. Ignore any "No Internet" warnings. You may also see a privacy warning — proceed anyway.

![Phone Wi-Fi and certificate settings](img/SettingForPhone.png)

3. Open a browser on your phone and navigate to the URL shown in the Serial Monitor (e.g., `https://192.168.X.X/2motor.html`).

4. Press **Activate** on the control panel and move the on-screen sliders. If numbers appear in the Serial Monitor, the connection is working.

![Phone control interface for 2-motor mode](img/IMG_7806.PNG)

![Serial Monitor showing motor values](img/2motermodeZero.png)

**Tip:** Turn off cellular data on your phone to prevent it from automatically switching away from the robot's network. Also disconnect from any VPN.

**Important:** Before disconnecting the motors, make sure both motor positions are set to **0**. Do not manually rotate the motors by hand. This is especially important before assembling the Tilty robot.

### Step 7: Audio

The robot can also produce sounds using a Bluetooth speaker.

**Connect a Bluetooth speaker:**

1. Power on the Bluetooth speaker.

![Bluetooth speaker power button](img/IMG_7758.png)

2. Pair a phone with the speaker via Bluetooth settings.

![Bluetooth pairing on phone](img/IMG_8803.PNG)

**Record and play sounds:**

1. On your phone's browser, navigate to `https://{IP address}/sounds.html`. Allow microphone access if prompted.

![Sound page and microphone permission](img/IMG_8798.PNG)
![Microphone access prompt](img/IMG_8797.PNG)

2. Press **Record** to start recording. A timer will count up. Press **Stop** to finish.

![Recording in progress](img/IMG_8799.PNG)

3. Name the recording in the popup and press **OK**.

4. Press the **Play** button to play back your recording.

![Playback controls](img/IMG_8800.PNG)

**Note:** Refreshing the control page will erase recorded sounds.

---

## 3. Hardware Assembly

Choose one of the two robot types to build:

| Driving Robot | Tilty Robot |
|---|---|
| Mobile robot on wheels | Dashboard robot that tilts and pans |

### Base Assembly

Assemble the core components (TinyPICO + Dynamixel motors) as shown below, then connect to your laptop via USB-A to USB-C cable.

![Base robot assembly](img/IMG_7787.JPG)

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

5. **Upload Drive mode:** Edit `platformio.ini` to enable `+<drive.cpp>` and comment out the others. Upload the firmware.

![Drive mode platformio.ini and upload](img/drivemode_upload.png)

**Note:** The URL path changes depending on the mode (`/drive.html`, `/2motor.html`, `/tilty.html`):

![URL changes per mode](img/URL.png)

6. **Connect your phone:** Join the robot's Wi-Fi network and navigate to the URL shown in the Serial Monitor (e.g., `https://192.168.X.X/drive.html`).

![Drive mode IP address in Serial Monitor](img/Drivemode_IPaddress.png)

7. **Drive:** Press Activate and use the on-screen joystick to control the robot.

![Drive mode in action](img/drive_mode2.gif)

**Prerequisite:** Before building the Tilty robot, use 2-motor mode to set both motors to position 0.

1. **Set motors to zero:** Connect to the 2-motor control page and set both motor positions to 0.

![Motor zero position on phone](img/MoterZeroScreenshot.png)
![Motor zero confirmed in Serial Monitor](img/2motermodeZero.png)

2. **Attach the head:** Screw the hinge bracket to **Motor 1** using the short screws. The "head" should face forward while at position 0.

![Head attached to Motor 1](img/IMG_7811.JPG)
![Bracket screws](img/screws_blaket.jpg)

3. **Attach the idler:** On **Motor 2**, attach only the idler and idler cap.

![Idler screws](img/screws_idler.jpg)

4. **Combine:** Attach the head/hinge assembly to Motor 2 in the correct orientation (the LED on top of Motor 2 should face you). **Do not rotate the wheels manually!**

![Correct assembly orientation](img/IMG_7789.jpg)

5. **Upload Tilty mode:** Edit `platformio.ini` to enable `+<tilty.cpp>` and comment out the others. Upload the firmware.

![Tilty mode upload](img/Tilty_upload.png)

6. **Connect your phone:** Join the Wi-Fi network and navigate to the URL shown in the Serial Monitor.

7. **Play:** Press Activate and tilt/pan the robot using the on-screen controls or your phone's gyroscope.

![Tilty mode in action](img/PanTilt_1.gif)

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
2. **Players:** Who is involved? Think about all the people present — the primary user, bystanders, children, roommates, etc. Poor designs often fail because they don't account for everyone in the environment (for example, voice assistants that didn't anticipate children or roommates placing unwanted orders).
3. **Activity:** What happens between the players and the robot?
4. **Goals:** What is each player trying to accomplish? (e.g., getting a notification, navigating a space, expressing an emotion)

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
