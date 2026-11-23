# Robot Arm ESP32 – Robot Arm Firmware

**Author:** Sonny BASSO
**Made for:** EPFL Robopoly kits
**License:** MIT (see [LICENSE](LICENSE))

Firmware for the **5-servo robot arm**, built around an ESP32-S3. The arm receives its commands wirelessly over ESP-NOW (a direct ESP32-to-ESP32 radio link, no WiFi network needed) from the **JIG controller**. Two controller firmwares can drive it:

- `firmware/controller_console`: controller **with** the Game extension (OLED screen), menu entry **5. ROBOT CONTROL**;
- `firmware/controller_basic`: controller **without** a screen, robot control only.

---

## How it works

- The controller sends the 5 target angles about every 50 ms.
- The arm moves **smoothly** toward each target (3° max every 20 ms) instead of jumping.
- Every 100 ms the arm sends a status message back to the controller. This is what shows `Conn: OK` on the controller screen (or `Bras: OK` in the Serial Monitor of the screenless version).
- **Safety mode:** if no valid command arrives for **2 seconds** (controller off, out of range or back in the menu), the status LED turns on and the arm returns slowly to its rest position (all servos at 90°). It resumes as soon as a new command arrives.
- Every command is checked: angles must be 0–180° and the checksum must match. Invalid messages are ignored.

---

## Hardware and pins

| Function | GPIO |
|---|---|
| Servo 1 – Base | 1 |
| Servo 2 – Shoulder | 2 |
| Servo 3 – Elbow | 3 |
| Servo 4 – Wrist | 4 |
| Servo 5 – Gripper | 5 |
| Status LED (ON = safety mode / no controller) | 8 |

### ⚠️ Power and safety
- **Don't power the servos from the ESP32's USB port.** Five servos can draw several amps when they move. Use a separate 5–6 V supply sized for your servos, and **connect its ground to the ESP32's GND**.
- **At power-up, every servo moves straight to 90°.** Keep hands and objects clear of the arm when you turn it on.

---

## Software setup

1. Install **[VS Code](https://code.visualstudio.com/)** and the **PlatformIO IDE** extension.
2. In PlatformIO, choose **Open Project** and select this folder (`firmware/arm`).
3. On the first build, PlatformIO downloads the ESP32 toolchain and the `ESP32Servo` library.
4. Connect the arm's ESP32 over USB, then click **Upload**.
5. Open the **Serial Monitor**. The speed is already set to **115200 baud** in `platformio.ini`.

Board: `esp32-s3-devkitm-1` · Framework: Arduino.

---

## Configuring MAC addresses (required)

The arm and the controller each need the **MAC address** (the unique hardware ID) of the other. The addresses in the code are the creator's boards, so **you must replace them with yours**.

### Step 1: Read the arm's MAC
Upload this project, open the Serial Monitor (115200 baud), press **RESET** and write down the line:
```
MAC du bras: XX:XX:XX:XX:XX:XX
```

### Step 2: Read the controller's MAC
- **With screen** (`firmware/controller_console`): upload it, open the Serial Monitor, launch **5. ROBOT CONTROL** and write down the line `MAC Address: XX:XX:XX:XX:XX:XX`.
- **Without screen** (`firmware/controller_basic`): upload it, open the Serial Monitor, press **RESET** and write down the line `MAC de la manette: XX:XX:XX:XX:XX:XX`.

Both firmwares show the same MAC on the same controller, because it belongs to the ESP32 chip.

### Step 3: Put the addresses in the code
Convert each address by adding `0x` in front of every pair:
```
98:88:E0:A7:6F:9C   →   {0x98, 0x88, 0xE0, 0xA7, 0x6F, 0x9C}
```

| File | Variable | Put the MAC of… |
|---|---|---|
| `firmware/arm/src/main.cpp` | `controllerMAC` | the **controller** (step 2) |
| `firmware/controller_console/src/robot/robot_comm.cpp` (with screen) **or** `firmware/controller_basic/src/main.cpp` (without screen) | `robotArmMAC` | the **arm** (step 1) |

Both lines are marked `-> À REMPLACER` in the code. **Re-upload both projects** after editing them.

### Step 4: Check
On the controller, launch ROBOT CONTROL and move a joystick:
- the arm's status LED turns **off**;
- the Serial Monitor of the arm shows `REÇU #…` lines;
- the controller screen shows `Conn: OK`.

### Troubleshooting

| Symptom | Likely cause |
|---|---|
| The arm doesn't move, its LED stays on | Wrong `robotArmMAC` in the controller project |
| The arm moves, but the controller shows `Conn: NONE` | Wrong `controllerMAC` in this project |
| `ERREUR: Ajout peer manette` at boot | Malformed MAC (it must be exactly 6 bytes) |
| Servos jitter or the ESP32 reboots when the arm moves | Servo power supply too weak, or grounds not connected together |

---

## Controller mapping (reminder)

| Control on the JIG controller | Servo |
|---|---|
| Joystick 1 horizontal | 1 – Base |
| Joystick 1 vertical | 2 – Shoulder |
| Joystick 2 vertical | 3 – Elbow |
| Joystick 2 horizontal | 4 – Wrist |
| Joystick 2 button | 5 – Gripper (open / close toggle) |

---

## Project structure

```
arm/
├── platformio.ini     PlatformIO configuration (board, ESP32Servo library, serial speed)
└── src/
    └── main.cpp       All of the arm's firmware: ESP-NOW, command checks, servo control
```

The `RobotCommand` and `RobotStatus` structures in `main.cpp` must stay **identical** to the ones in `firmware/controller_console/src/robot/robot_comm.h`. If you change one, change the other too.

---

## Known limitations

- `battery_level` and `connection_rssi` in the status message are fixed placeholder values (85 and 75). They aren't measured.
- ESP-NOW messages are neither encrypted nor authenticated, so any nearby ESP32 sending the right message format could move the arm. This is fine for a hobby kit, but don't use the protocol for anything safety-critical.
- Code comments and serial messages are in French.
