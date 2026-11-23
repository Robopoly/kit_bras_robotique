# Kit JIG – Game Controller Firmware

**Author:** Sonny BASSO
**Made for:** EPFL Robopoly kits
**License:** MIT (see [LICENSE](LICENSE))

Firmware for the **JIG handheld controller**, built around an ESP32-S3, a 128×64 OLED screen and two analog joysticks. At power-up it shows a menu of mini-games. It can also drive the companion **robot arm** (folder `Robot_Arm_ESP32`) over ESP-NOW, a direct ESP32-to-ESP32 radio link that doesn't need a WiFi network.

> **No screen?** This firmware needs the Game extension (OLED screen). For a controller PCB without it, use `Kit_JIG_robot_arm_controller` instead: it does robot arm control only.

---

## Features

| # | Menu entry | Description |
|---|---|---|
| 1 | PONG | Two players on one controller, or **multiplayer between two controllers** over ESP-NOW (Host / Client) |
| 2 | SPACE INVADERS | Classic shoot-'em-up |
| 3 | PAC-MAN | Maze game with ghosts |
| 4 | SNAKE | Classic snake |
| 5 | ROBOT CONTROL | Remote control of the 5-servo robot arm (`Robot_Arm_ESP32`) |
| 6 | DOOM | 3D raycaster: solo mode (random levels) or **deathmatch between two controllers** |
| 7 | DOOM (Ecran) | Turns the controller into a gamepad for a Waveshare ESP32-S3-Touch-LCD-2.8C screen (the screen's firmware is **not** included here) |

The battery level is shown as a percentage in the top-right corner of the menu.

**Menu navigation:** joystick 1 vertical scrolls the list, the joystick 2 button starts the selected entry.

---

## Hardware and pins

| Function | GPIO |
|---|---|
| OLED SSD1306 128×64 (I²C, address `0x3C`) | SDA = 8, SCL = 9 |
| Joystick 1 – vertical / horizontal / button | 1 / 2 / 5 |
| Joystick 2 – vertical / horizontal / button | 3 / 4 / 6 |
| Battery voltage (÷2 divider, ADC) | 10 |
| Vibration motors (held OFF at boot) | 17, 18 |

The screen is rotated 180° in software (`display.setRotation(2)` in `src/common/config.cpp`).

---

## Software setup

1. Install **[VS Code](https://code.visualstudio.com/)** and the **PlatformIO IDE** extension.
2. In PlatformIO, choose **Open Project** and select this folder (`Kit_JIG_controller_test`).
3. On the first build, PlatformIO downloads the ESP32 toolchain and the libraries (Adafruit GFX and Adafruit SSD1306). This can take a few minutes.
4. Connect the controller over USB, then click **Upload** (→ arrow in the bottom bar).
5. Open the **Serial Monitor** (plug icon). The speed is already set to **115200 baud** in `platformio.ini`.

Board: `esp32-s3-devkitm-1` · Framework: Arduino.

---

## Configuring MAC addresses (required for ROBOT CONTROL)

The controller and the robot arm talk directly to each other, and each one needs the **MAC address** (the unique hardware ID) of the other. The addresses in the code are the creator's boards, so **you must replace them with yours**. Otherwise the arm won't respond.

### Step 1: Read the arm's MAC
1. Open the `Robot_Arm_ESP32` project and upload it to the arm's ESP32.
2. Open the Serial Monitor (115200 baud) and press the board's **RESET** button.
3. Write down the line `MAC du bras: XX:XX:XX:XX:XX:XX`.

### Step 2: Read the controller's MAC
1. Upload this project (`Kit_JIG_controller_test`) to the controller.
2. Open the Serial Monitor (115200 baud) and launch **5. ROBOT CONTROL** from the menu.
3. Write down the line `MAC Address: XX:XX:XX:XX:XX:XX`.

### Step 3: Put the addresses in the code
Convert each address by adding `0x` in front of every pair:

```
FC:01:2C:08:FC:9C   →   {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}
```

| File | Variable | Put the MAC of… |
|---|---|---|
| `Kit_JIG_controller_test/src/robot/robot_comm.cpp` | `robotArmMAC` | the **arm** (step 1) |
| `Robot_Arm_ESP32/src/main.cpp` | `controllerMAC` | the **controller** (step 2) |

Both lines are marked `-> À REMPLACER` in the code. **Re-upload both projects** after editing them.

### Step 4: Check
Launch ROBOT CONTROL. The bottom of the screen should show `Conn: OK`, and the arm's status LED should turn off as soon as you move a joystick.

> `controllerMAC` in `robot_comm.cpp` isn't used by the controller itself. It's only there for reference.

### ROBOT CONTROL mapping

| Control | Servo |
|---|---|
| Joystick 1 horizontal | 1 – Base |
| Joystick 1 vertical | 2 – Shoulder |
| Joystick 2 vertical | 3 – Elbow |
| Joystick 2 horizontal | 4 – Wrist |
| Joystick 2 button | 5 – Gripper (open / close toggle) |
| Joystick 1 button | Back to menu |

---

## Wireless multiplayer (PONG / DOOM)

Multiplayer modes **don't need any MAC configuration**. Each controller broadcasts a discovery message and pairs automatically with the **first controller that answers**.

⚠️ In a room with many kits, you may pair with your neighbour instead of your friend. Start the multiplayer mode on both controllers at about the same time, ideally away from other kits.

---

## Project structure

```
Kit_JIG_controller_test/
├── platformio.ini          PlatformIO configuration (board, libraries, serial speed)
├── src/
│   ├── main.cpp            Main loop: dispatches between menu, games and robot control
│   ├── common/             Pin definitions, OLED and joystick initialisation
│   ├── menu/               Main menu (navigation, battery display)
│   ├── games/              Pong, Space Invaders, Pac-Man, Snake, DOOM, DOOM controller
│   └── robot/              ESP-NOW protocol (robot_comm) and robot arm control (robot_control)
└── python_simulator/       Space Invaders simulator for PC (Python + pygame)
```

### Python simulator
`python_simulator/` lets you try Space Invaders on a PC without the hardware:

```bash
cd python_simulator
pip install pygame
python space_invaders_simulator.py
```

See [python_simulator/README.md](python_simulator/README.md) for details (in French).

---

## Known limitations

- The battery percentage is an approximate calibration (3.0 V → 0 %, 4.2 V → 100 %) and may need tuning for your battery.
- ESP-NOW messages are neither encrypted nor authenticated. This is fine for a hobby kit, but don't use the protocol for anything safety-critical.
- Wireless pairing in PONG and DOOM has no pairing code (see above).
- Code comments and serial messages are mostly in French.
