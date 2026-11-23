#ifndef DOOM_CTRL_H
#define DOOM_CTRL_H

/*
 * doom_ctrl.h -- Kit_JIG controller mode for the Waveshare DOOM display.
 *
 * Instead of running the raycaster locally on the OLED, this mode sends
 * joystick + button state to the Waveshare ESP32 via ESP-NOW at ~20 Hz.
 * The Waveshare runs the actual game and renders it on its 480x480 screen.
 *
 * Protocol (message bytes shared with doom_game.cpp multiplayer):
 *   0xA0  broadcast discovery → Waveshare picks it up and replies 0xA1
 *   0xC0  controller input packet (20 Hz)  → Waveshare
 *   0xC1  ack / heartbeat                  ← Waveshare
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_SSD1306.h>

/* ---------- message types ---------- */
#define DOOM_CTRL_MSG_DISCOVERY     0xA0
#define DOOM_CTRL_MSG_DISCOVERY_ACK 0xA1
#define DOOM_CTRL_MSG_INPUT         0xC0
#define DOOM_CTRL_MSG_ACK           0xC1

#pragma pack(push, 1)
struct DoomCtrlInput {
    uint8_t  msgType;   /* DOOM_CTRL_MSG_INPUT */
    int16_t  joy1V;     /* forward / back  [-2048..+2048] */
    int16_t  joy1H;     /* strafe          [-2048..+2048] */
    int16_t  joy2H;     /* rotate          [-2048..+2048] */
    uint8_t  btnA;      /* quit  (1=pressed) */
    uint8_t  btnB;      /* shoot (1=pressed) */
    uint32_t timestamp;
};

struct DoomCtrlAck {
    uint8_t  msgType;
    uint8_t  gameState;
    uint32_t timestamp;
};
#pragma pack(pop)

class DoomCtrl {
public:
    DoomCtrl(Adafruit_SSD1306 *disp);

    void init();
    void update(int joy1V, int joy1H, int joy2V, int joy2H,
                bool btnA, bool btnB);
    void draw();
    bool isRunning();
    bool shouldReturnToMenu();

    /* Called from static ESP-NOW receive callback */
    void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);

    static DoomCtrl *instance;

private:
    Adafruit_SSD1306 *display;

    bool running;
    bool connected;
    bool espNowInited;
    uint8_t peerMAC[6];

    unsigned long lastDiscovery;
    unsigned long lastSent;
    unsigned long lastAck;
    unsigned long initTime;

    static const unsigned long DISCOVERY_INTERVAL = 500;
    static const unsigned long SEND_INTERVAL      = 50;   /* 20 Hz */
    static const unsigned long PEER_TIMEOUT       = 3000;

    static void onRecvStatic(const uint8_t *mac, const uint8_t *data, int len);
    static void onSentStatic(const uint8_t *mac, esp_now_send_status_t st);

    void sendDiscovery();
    void sendInput(int joy1V, int joy1H, int joy2H, bool btnA, bool btnB);
    void deinit();
};

#endif
