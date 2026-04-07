/*
 * XL330 Homing Offset & Position Limit Test
 *
 * Standalone test to verify we can:
 * 1. Read current position
 * 2. Set Homing Offset so current position becomes "center" (2048)
 * 3. Set Min/Max Position Limits
 * 4. Verify the motor refuses to go past limits
 *
 * Connect one or two motors, open serial monitor, follow prompts.
 * Flash with: pio run -e calibration_test -t upload
 */

#include <Arduino.h>
#include "XL330.h"

#define RXD2 2
#define TXD2 1
#define BROADCAST 254
#define POSITION_MODE 3

// XL330 EEPROM addresses
#define XL_HOMING_OFFSET      20   // 4 bytes, signed
#define XL_MAX_POSITION_LIMIT  48  // 4 bytes
#define XL_MIN_POSITION_LIMIT  52  // 4 bytes

XL330 robot;

void writeEeprom4(int id, int address, int32_t value) {
    // Must have torque off to write EEPROM
    robot.sendPacket_4bytes(id, address, value);
    Serial2.flush();
    delay(100);
    // Drain any status packet response
    while (Serial2.available()) Serial2.read();
}

int32_t readPositionDebug(int id) {
    // Drain buffer
    int drained = 0;
    while (Serial2.available()) { Serial2.read(); drained++; }
    if (drained > 0) Serial.printf("  [debug] drained %d bytes\n", drained);
    delay(20);

    // Send read request manually to see what comes back
    int sent = robot.RXsendPacket(id, 132, 4); // 132 = XL_PRESENT_POSITION
    Serial2.flush();
    Serial.printf("  [debug] sent %d bytes, waiting...\n", sent);
    delay(50);

    // Read raw bytes
    int avail = Serial2.available();
    Serial.printf("  [debug] %d bytes available: ", avail);
    unsigned char buf[64];
    int n = 0;
    while (Serial2.available() && n < 64) {
        buf[n++] = Serial2.read();
    }
    for (int i = 0; i < n; i++) Serial.printf("%02X ", buf[i]);
    Serial.println();
    
    // Try to parse as packet
    if (n > 0) {
        XL330::Packet p(buf, n);
        Serial.printf("  [debug] valid=%s id=%d instr=0x%02X params=%d\n",
            p.isValid()?"yes":"no", p.getId(), p.getInstruction(), p.getParameterCount());
    }
    return -1; // always fail for now, just debugging
}

int32_t readPosition(int id) {
    // The half-duplex bus echoes our TX bytes back on RX.
    // getJointPosition sends 14 bytes (read request), then tries to
    // parse the response. But it reads the 14-byte echo first and
    // fails because the echo has instruction 0x02, not 0x55.
    // Fix: send the request ourselves, skip the echo, then parse.
    while (Serial2.available()) Serial2.read(); // drain
    delay(10);

    int sent = robot.RXsendPacket(id, 132, 4); // 132 = Present Position
    Serial2.flush();
    delay(20);

    // Skip the TX echo (same number of bytes we sent)
    unsigned long start = millis();
    int skipped = 0;
    while (skipped < sent && millis() - start < 200) {
        if (Serial2.available()) { Serial2.read(); skipped++; }
    }

    // Now read the actual response
    unsigned char buffer[64];
    if (robot.readPacket(buffer, 64) > 0) {
        XL330::Packet p(buffer, 64);
        if (p.isValid() && p.getParameterCount() >= 3 && p.getInstruction() == 0x55) {
            return (p.getParameter(1)) |
                   (p.getParameter(2) << 8) |
                   (p.getParameter(3) << 16) |
                   (p.getParameter(4) << 24);
        }
    }
    return -1;
}

void drainSerial() {
    Serial2.flush();
    delay(20);
    while (Serial2.available()) Serial2.read();
}

void printMotorInfo(int id) {
    int32_t pos = readPosition(id);
    Serial.printf("  Motor %d: position = %d (%.1f degrees)\n", id, pos, pos * 0.088);
}

void testMotor(int id) {
    Serial.printf("\n=== Testing Motor %d ===\n", id);

    // Step 1: Read current position
    int32_t currentPos = readPosition(id);
    if (currentPos < 0) {
        Serial.printf("  ERROR: Can't read motor %d (got %d). Is it connected?\n", id, currentPos);
        return;
    }
    Serial.printf("  Current position: %d (%.1f deg)\n", currentPos, currentPos * 0.088);

    // Step 2: Calculate homing offset to make current position = 2048
    int32_t offset = 2048 - currentPos;
    Serial.printf("  Homing offset needed: %d\n", offset);

    // Step 3: Write homing offset (torque must be off)
    Serial.println("  Writing homing offset...");
    robot.TorqueOFF(id);
    delay(100);
    writeEeprom4(id, XL_HOMING_OFFSET, offset);
    delay(100);

    // Cycle torque so the offset takes effect
    robot.TorqueON(id);
    drainSerial();
    delay(100);
    robot.TorqueOFF(id);
    drainSerial();
    delay(100);

    // Step 4: Verify - read position again, should be ~2048
    int32_t newPos = readPosition(id);
    Serial.printf("  Position after offset: %d (should be ~2048)\n", newPos);

    // Step 5: Set position limits (e.g., 1024 to 3072 = 180 degree range)
    int32_t minLimit = 1024;
    int32_t maxLimit = 3072;
    Serial.printf("  Setting limits: min=%d, max=%d\n", minLimit, maxLimit);
    writeEeprom4(id, XL_MIN_POSITION_LIMIT, minLimit);
    writeEeprom4(id, XL_MAX_POSITION_LIMIT, maxLimit);
    delay(100);

    // Step 6: Enable torque and test limits
    robot.setControlMode(id, POSITION_MODE);
    drainSerial();
    robot.TorqueON(id);
    drainSerial();

    Serial.println("  Moving to center (2048)...");
    robot.setJointPosition(id, 2048);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    Serial.println("  Moving to min limit (1024)...");
    robot.setJointPosition(id, 1024);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    Serial.println("  Moving to max limit (3072)...");
    robot.setJointPosition(id, 3072);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    Serial.println("  Trying PAST max (3500) - should be clamped...");
    robot.setJointPosition(id, 3500);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    Serial.println("  Trying PAST min (500) - should be clamped...");
    robot.setJointPosition(id, 500);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    // Return to center
    Serial.println("  Back to center...");
    robot.setJointPosition(id, 2048);
    drainSerial();
    delay(1500);
    printMotorInfo(id);

    Serial.println("  Done!");
}

void resetMotor(int id) {
    Serial.printf("\n=== Resetting Motor %d to defaults ===\n", id);
    robot.TorqueOFF(id);
    delay(100);
    writeEeprom4(id, XL_HOMING_OFFSET, 0);
    writeEeprom4(id, XL_MIN_POSITION_LIMIT, 0);
    writeEeprom4(id, XL_MAX_POSITION_LIMIT, 4095);
    delay(100);
    Serial.println("  Homing offset = 0, limits = 0-4095");
    printMotorInfo(id);
    Serial.println("  Done!");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
    Serial2.setTimeout(500);
    Serial2.flush();
    robot.begin(Serial2);

    // Init motors same as tiltybot firmware
    robot.TorqueOFF(BROADCAST);
    delay(100);
    drainSerial();
    robot.setControlMode(BROADCAST, POSITION_MODE);
    delay(100);
    drainSerial();
    robot.TorqueON(BROADCAST);
    delay(100);
    drainSerial();

    Serial.println("\n========================================");
    Serial.println("  XL330 HOMING OFFSET & LIMIT TEST");
    Serial.println("========================================");
    Serial.println("\nCommands:");
    Serial.println("  1 - Test motor 1 (set offset + limits)");
    Serial.println("  2 - Test motor 2 (set offset + limits)");
    Serial.println("  r - Read both motor positions");
    Serial.println("  x - Reset both motors to defaults");
    Serial.println("  h - Show this help");
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        switch (c) {
            case '1':
                testMotor(1);
                break;
            case '2':
                testMotor(2);
                break;
            case 'r':
                Serial.println("\n--- Current positions ---");
                printMotorInfo(1);
                printMotorInfo(2);
                break;
            case 'x':
                resetMotor(1);
                resetMotor(2);
                break;
            case 'd':
                Serial.println("\n--- Debug read motor 2 ---");
                readPositionDebug(2);
                Serial.println("--- Debug read motor 1 ---");
                readPositionDebug(1);
                break;
            case 'h':
                Serial.println("\n1=test motor1, 2=test motor2, r=read, d=debug, x=reset, h=help");
                break;
        }
    }
    delay(10);
}
