# Kit JIG – Robot Arm Controller (no screen)

**Author:** Sonny BASSO
**Made for:** EPFL Robopoly kits
**License:** MIT (see [LICENSE](LICENSE))

Firmware for the **JIG controller PCB without the "Game" extension**, meaning no OLED screen. It does just one thing: control the 5-servo **robot arm** (folder `firmware/arm`) with the two joysticks over ESP-NOW, a direct ESP32-to-ESP32 radio link that doesn't need a WiFi network.

It starts as soon as the controller is powered on. There's no menu and no games. All information (MAC address, connection state, angles) is shown in the **Serial Monitor**.

> If you have the Game extension (OLED screen), use `firmware/controller_console` instead. It includes this same robot control as menu entry **5. ROBOT CONTROL**, plus the games.

---

## Controls

| Control | Action |
|---|---|
| Joystick 1 horizontal | Servo 1 – Base |
| Joystick 1 vertical | Servo 2 – Shoulder |
| Joystick 2 vertical | Servo 3 – Elbow |
| Joystick 2 horizontal | Servo 4 – Wrist |
| Joystick 2 button | Servo 5 – Gripper (open / close toggle) |
| Joystick 1 button | **Rest position:** servos 1–4 go back to 90° (the gripper doesn't move) |

The further you push a joystick, the faster the servo turns.

⚠️ **Don't touch the joysticks while the controller powers on.** For about 0.1 s at startup, the firmware measures each joystick's centre position. If a joystick is pushed during that time, the matching servo will drift on its own. Press **RESET** to calibrate again.

---

## Hardware and pins

| Function | GPIO |
|---|---|
| Joystick 1 – vertical / horizontal / button | 1 / 2 / 5 |
| Joystick 2 – vertical / horizontal / button | 3 / 4 / 6 |
| Vibration motors (held OFF) | 17, 18 |

These pins are the same as in `firmware/controller_console`, so you can switch between the two firmwares on the same PCB at any time.

---

## Software setup

1. Install **[VS Code](https://code.visualstudio.com/)** and the **PlatformIO IDE** extension.
2. In PlatformIO, choose **Open Project** and select this folder (`firmware/controller_basic`).
3. Connect the controller over USB, then click **Upload** (→ arrow in the bottom bar).
4. Open the **Serial Monitor** (plug icon). The speed is already set to **115200 baud** in `platformio.ini`.

Board: `esp32-s3-devkitm-1` · Framework: Arduino · No external library needed.

---

## Configuring MAC addresses (required)

The controller and the arm each need the **MAC address** (the unique hardware ID) of the other. The addresses in the code are the creator's boards, so **you must replace them with yours**. Otherwise the arm won't respond.

### Step 1: Read the arm's MAC
1. Open the `firmware/arm` project and upload it to the arm's ESP32.
2. Open the Serial Monitor (115200 baud) and press the board's **RESET** button.
3. Write down the line `MAC du bras: XX:XX:XX:XX:XX:XX`.

### Step 2: Read the controller's MAC
1. Upload this project to the controller.
2. Open the Serial Monitor (115200 baud) and press **RESET**.
3. Write down the line `MAC de la manette: XX:XX:XX:XX:XX:XX`.

### Step 3: Put the addresses in the code
Convert each address by adding `0x` in front of every pair:

```
FC:01:2C:08:FC:9C   →   {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}
```

| File | Variable | Put the MAC of… |
|---|---|---|
| `firmware/controller_basic/src/main.cpp` | `robotArmMAC` | the **arm** (step 1) |
| `firmware/arm/src/main.cpp` | `controllerMAC` | the **controller** (step 2) |

Both lines are marked `-> À REMPLACER` in the code. **Re-upload both projects** after editing them.

> The MAC belongs to the ESP32 chip, not to the firmware. If you already configured the arm for `firmware/controller_console`, the controller's MAC is the same and nothing needs to change on the arm.

### Step 4: Check
With both boards powered on, the controller's Serial Monitor shows:
```
>>> Bras CONNECTÉ
Angles: [90,90,90,90,90] | Bras: OK
```
The arm's status LED turns **off** as soon as it receives commands.

### Troubleshooting

| Symptom | Likely cause |
|---|---|
| The arm doesn't move, its LED stays on | Wrong `robotArmMAC` in this project |
| The arm moves, but the Serial Monitor shows `Bras: NONE` | Wrong `controllerMAC` in `firmware/arm` |
| `ERREUR: Ajout peer bras` at boot | Malformed MAC (it must be exactly 6 bytes) |
| A servo drifts on its own without touching the joysticks | A joystick was pushed at startup: press RESET without touching it |
| Unreadable characters in the Serial Monitor | Wrong speed: select 115200 baud |

---

## How it works

- Every 50 ms the controller reads the joysticks and updates the 5 target angles. It sends them to the arm whenever they change, and at least every 200 ms otherwise.
- That regular sending matters: if the arm receives nothing for 2 seconds, it goes into **safety mode** (LED on, slow return to 90°).
- The arm replies with a status message every 100 ms. If none arrives for 1 second, the controller reports `Bras DÉCONNECTÉ`.
- The `RobotCommand` and `RobotStatus` structures in `main.cpp` must stay **identical** to the ones in `firmware/arm/src/main.cpp`.

---

## Project structure

```
controller_basic/
├── platformio.ini     PlatformIO configuration (board, serial speed)
└── src/
    └── main.cpp       All of the firmware: joysticks, ESP-NOW, connection monitoring
```

---

## Known limitations

- There's no visual feedback on the controller itself. Use the Serial Monitor, or watch the arm's LED.
- ESP-NOW messages are neither encrypted nor authenticated. This is fine for a hobby kit, but don't use the protocol for anything safety-critical.
- Code comments and serial messages are in French.
