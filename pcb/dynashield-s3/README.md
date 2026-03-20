# Dynashield S3

PCB adapter board for mounting a Waveshare ESP32-S3-Zero underneath, with a Dynamixel XL330 connector on top.

## Setup

1. Download the ESP32-S3-Zero part from [SnapEDA](https://www.snapeda.com/parts/ESP32-S3-Zero/Waveshare%20Electronics/view-part/)
2. Import the `.kicad_sym` and `.kicad_mod` files into KiCad (Preferences → Manage Symbol/Footprint Libraries → Add)
3. Open `dynashield-s3.kicad_sch` in KiCad
4. Add the ESP32-S3-Zero symbol and the B3B-EH-ALFSN connector (from KiCad's built-in Connector library)

## Wiring

| ESP32-S3-Zero | Net | Dynamixel Connector |
|---|---|---|
| GPIO1 (pin 4, left side) | DATA | Pin 3 (Data) |
| GPIO2 (pin 5, left side) | DATA | Pin 3 (Data) |
| 5V (pin 1, top left) | VDD | Pin 2 (VDD) |
| GND (pin 2, top left) | GND | Pin 1 (GND) |

GPIO1 and GPIO2 are tied together on the PCB — they share the same net (DATA) which connects to the Dynamixel's half-duplex data line.

## Board layout

The ESP32-S3-Zero mounts on the **bottom** of the PCB using its castellated hole pads (pins 1 and 2 side — 5V, GND, 3V3, GP1-GP6). The Dynamixel connector sits on the **top**.
