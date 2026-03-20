/*
 * TiltyBot Motor Setup Tool
 *
 * Interactive serial tool to configure Dynamixel XL330 motors.
 * Connect ONE motor at a time, type its ID (1 or 2).
 * Once both are configured, daisy-chain them and type 't' to test.
 *
 * Flash with: pio run -e motor_setup -t upload
 */

#include <Arduino.h>
#include "XL330.h"

#define RXD2 2
#define TXD2 1
#define BROADCAST 254
#define POSITION_MODE 3
#define DRIVE_MODE 16

XL330 robot;

void initSerial(long baud) {
    Serial2.end();
    delay(100);
    Serial2.begin(baud, SERIAL_8N1, RXD2, TXD2);
    delay(100);
    robot.begin(Serial2);
}

void configureMotor(int id) {
    Serial.printf("\nConfiguring as Motor %d...\n", id);

    // Reset at all common bauds
    int bauds[] = {57600, 115200, 1000000};
    for (int i = 0; i < 3; i++) {
        initSerial(bauds[i]);
        robot.TorqueOFF(BROADCAST);
        delay(50);
        robot.setBaudRate(BROADCAST, 1); // 57600
        delay(50);
    }

    // Set ID and baud
    initSerial(57600);
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setID(BROADCAST, id);
    delay(50);
    robot.setBaudRate(id, 2); // 115200
    delay(50);

    // Verify with blink + movement
    initSerial(115200);
    robot.LEDON(id);
    delay(1000);
    robot.LEDOFF(id);

    robot.TorqueOFF(id);
    delay(50);
    robot.setControlMode(id, POSITION_MODE);
    delay(50);
    robot.TorqueON(id);
    delay(50);

    robot.setJointPosition(id, 1024);
    delay(1500);
    robot.setJointPosition(id, 3072);
    delay(1500);
    robot.setJointPosition(id, 2048);
    delay(1000);

    Serial.printf("Motor %d configured! (ID=%d, 115200 baud)\n", id, id);
    Serial.println("Swap motor and reset board to configure the other.");
}

void testBothMotors() {
    Serial.println("\nTesting both motors...");
    initSerial(115200);

    Serial.println("Blink Motor 1...");
    robot.LEDON(1);
    delay(1000);
    robot.LEDOFF(1);
    delay(300);

    Serial.println("Blink Motor 2...");
    robot.LEDON(2);
    delay(1000);
    robot.LEDOFF(2);
    delay(300);

    // Position test
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setControlMode(BROADCAST, POSITION_MODE);
    delay(50);
    robot.TorqueON(BROADCAST);
    delay(50);

    Serial.println("Both to center...");
    robot.setJointPosition(1, 2048);
    delay(10);
    robot.setJointPosition(2, 2048);
    delay(2000);

    Serial.println("Motor 1 left, Motor 2 right...");
    robot.setJointPosition(1, 1024);
    delay(10);
    robot.setJointPosition(2, 3072);
    delay(2000);

    Serial.println("Swap...");
    robot.setJointPosition(1, 3072);
    delay(10);
    robot.setJointPosition(2, 1024);
    delay(2000);

    Serial.println("Back to center...");
    robot.setJointPosition(1, 2048);
    delay(10);
    robot.setJointPosition(2, 2048);
    delay(2000);

    // Drive test
    robot.TorqueOFF(BROADCAST);
    delay(50);
    robot.setControlMode(BROADCAST, DRIVE_MODE);
    delay(50);
    robot.TorqueON(BROADCAST);
    delay(50);

    Serial.println("Motor 1 slow (200)...");
    robot.setJointSpeed(1, 200);
    delay(2000);
    robot.setJointSpeed(1, 0);
    delay(500);

    Serial.println("Motor 2 slow (200)...");
    robot.setJointSpeed(2, 200);
    delay(2000);
    robot.setJointSpeed(2, 0);
    delay(500);

    Serial.println("Both slow forward...");
    robot.setJointSpeed(1, 200);
    delay(10);
    robot.setJointSpeed(2, 200);
    delay(2000);
    robot.setJointSpeed(1, 0);
    delay(10);
    robot.setJointSpeed(2, 0);
    delay(500);

    Serial.println("Both max forward (885)...");
    robot.setJointSpeed(1, 885);
    delay(10);
    robot.setJointSpeed(2, 885);
    delay(2000);

    Serial.println("Both max backward (-885)...");
    robot.setJointSpeed(1, -885);
    delay(10);
    robot.setJointSpeed(2, -885);
    delay(2000);

    Serial.println("Stop.");
    robot.setJointSpeed(1, 0);
    delay(10);
    robot.setJointSpeed(2, 0);

    Serial.println("\nAll tests done!");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("  TILTYBOT MOTOR SETUP");
    Serial.println("========================================");
    Serial.println("\nOptions:");
    Serial.println("  1 - Configure this motor as Motor 1");
    Serial.println("  2 - Configure this motor as Motor 2");
    Serial.println("  t - Test both motors (daisy-chained)");

    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '1' || c == '2') {
                configureMotor(c - '0');
                return;
            }
            if (c == 't' || c == 'T') {
                testBothMotors();
                return;
            }
        }
        delay(10);
    }
}

void loop() {
    delay(1000);
}
