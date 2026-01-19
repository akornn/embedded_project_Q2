| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- |

# Navigate Without Sight

An ESP32-based assistive navigation system that helps a blindfolded user
detect and avoid obstacles using distance and motion sensors.

## Project Goal

The goal of this project is to assist a person without visual input by
detecting nearby obstacles and providing real-time feedback using sound
and motion cues.


## Features

- Obstacle detection using ultrasonic and ToF sensors
- Orientation and motion tracking via IMU
- Real-time feedback using buzzer and servo motor
- Modular ESP-IDF component-based architecture
- FreeRTOS task-based design


## Components 
- ESP32 (DevKit V1)
- VL53L1x Time of Fligh sensor (up to 4m, i2c)
- 10DOF GY87 Gyroscope/Accelerometer (i2c)
- HC-SR04P Ultrasonic Distance sensor
- SG90 Micro servo (180°)
- Active buzzer

## Software & Tools

- ESP-IDF (v5.x recommended)
- C language
- Visual Studio Code
- ESP-IDF VS Code Extension
- FreeRTOS

## Code Structure

```text
├── components/
│   ├── buzzer/         # Buzzer control logic
│   ├── gyro/           # 10DOF GY87 Accelerometer/Gyroscope (I2C)
│   ├── servo/          # Servo motor control
│   ├── ultrasonic/     # HC-SR04 Ultrasonic sensor
│   └── tof/            # VL53L1x ToF (I2C)
├── main/
│   ├── main.c          # Entry point and Task Orchestrator
│   └── CMakeLists.txt  # Project-level build configuration
└── README.md

```


## System Overview

Each sensor is implemented as an independent ESP-IDF component.
FreeRTOS tasks run in parallel to:

1. Measure distances
2. Track orientation
3. Decide feedback behavior
4. Alert the user via sound and movement


## Usage

Each component exposes a simple API through its header file.
Components are initialized in `main.c` and used inside FreeRTOS tasks.

## Build and Flash

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py flash monitor


```

## Configuration

- I2C pins are defined in the gyro component
- GPIO pins for ultrasonic, servo, and buzzer are configurable

## Contributors

- Adrians Doņeckis
- Aleksandar Mitovski
- Oskar Lukáč
- Raphael Vasilišin
- Tieme van Rees

## License

This project is for educational purposes.


