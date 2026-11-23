#include "doom_ctrl.h"
#include "../common/config.h"
#include <string.h>

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

DoomCtrl *DoomCtrl::instance = nullptr;

DoomCtrl::DoomCtrl(Adafruit_SSD1306 *disp)
    : display(disp), running(false), connected(false),
      espNowInited(false), lastDiscovery(0), lastSent(0), lastAck(0), initTime(0) {
    instance = this;
    memset(peerMAC, 0, 6);
}

/* -----------------------------------------------------------------------
 *  ESP-NOW callbacks
 * ----------------------------------------------------------------------- */
void DoomCtrl::onSentStatic(const uint8_t *mac, esp_now_send_status_t st) {
    (void)mac; (void)st;
}

void DoomCtrl::onRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
    if (instance) instance->onDataReceived(mac, data, len);
}

void DoomCtrl::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    if (!data || len < 1) return;
    uint8_t type = data[0];

    if (type == DOOM_CTRL_MSG_DISCOVERY_ACK || type == DOOM_CTRL_MSG_ACK) {
        /* Waveshare confirmed -- register as unicast peer */
        if (!connected) {
            memcpy(peerMAC, mac, 6);
            connected = true;
            if (!esp_now_is_peer_exist(peerMAC)) {
                esp_now_peer_info_t pi = {};
                memcpy(pi.peer_addr, peerMAC, 6);
                pi.channel = 0;
                pi.encrypt = false;
                esp_now_add_peer(&pi);
            }
        }
        lastAck = millis();
    }
}

/* -----------------------------------------------------------------------
 *  Private helpers
 * ----------------------------------------------------------------------- */
void DoomCtrl::sendDiscovery() {
    if (!espNowInited) return;
    uint8_t buf[2] = { DOOM_CTRL_MSG_DISCOVERY, 0x01 };
    esp_now_send(BROADCAST_MAC, buf, 2);
}

void DoomCtrl::sendInput(int joy1V, int joy1H, int joy2H,
                          bool btnA, bool btnB) {
    if (!espNowInited || !connected) return;
    DoomCtrlInput pkt;
    pkt.msgType   = DOOM_CTRL_MSG_INPUT;
    pkt.joy1V     = (int16_t)joy1V;
    pkt.joy1H     = (int16_t)joy1H;
    pkt.joy2H     = (int16_t)joy2H;
    pkt.btnA      = btnA ? 1 : 0;
    pkt.btnB      = btnB ? 1 : 0;
    pkt.timestamp = millis();
    esp_now_send(peerMAC, (uint8_t*)&pkt, sizeof(pkt));
}

void DoomCtrl::deinit() {
    if (!espNowInited) return;
    if (esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_del_peer(BROADCAST_MAC);
    if (connected && esp_now_is_peer_exist(peerMAC)) esp_now_del_peer(peerMAC);
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    espNowInited = false;
    connected    = false;
}

/* -----------------------------------------------------------------------
 *  Public API
 * ----------------------------------------------------------------------- */
void DoomCtrl::init() {
    instance      = this;
    running       = true;
    connected     = false;
    lastDiscovery = 0;
    lastSent      = 0;
    lastAck       = 0;
    initTime      = millis();
    memset(peerMAC, 0, 6);

    if (espNowInited) deinit();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);

    /* Force-deinit in case another mode (robot control, local DOOM) left
       ESP-NOW initialized without our espNowInited flag knowing about it. */
    esp_now_deinit();

    if (esp_now_init() != ESP_OK) {
        Serial.println("DoomCtrl: esp_now_init FAILED");
        running = false;
        return;
    }
    esp_now_register_recv_cb(onRecvStatic);
    esp_now_register_send_cb(onSentStatic);

    /* Add broadcast peer for discovery sends */
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
        esp_now_peer_info_t pi = {};
        memcpy(pi.peer_addr, BROADCAST_MAC, 6);
        pi.channel = 0;
        pi.encrypt = false;
        esp_now_add_peer(&pi);
    }

    espNowInited = true;
    Serial.println("DoomCtrl: initialized, broadcasting for Waveshare...");
}

void DoomCtrl::update(int joy1V, int joy1H, int joy2V, int joy2H,
                       bool btnA, bool btnB) {
    if (!running) return;
    unsigned long now = millis();

    /* Quit when button A pressed -- but only after 500ms grace period to
       prevent the menu select button bleed-through from triggering an exit. */
    if (btnA && !connected && (now - initTime > 500)) {
        deinit();
        running = false;
        return;
    }

    /* Send discovery broadcast until connected */
    if (!connected && now - lastDiscovery > DISCOVERY_INTERVAL) {
        sendDiscovery();
        lastDiscovery = now;
    }

    /* Peer timeout */
    if (connected && now - lastAck > PEER_TIMEOUT) {
        connected = false;
        Serial.println("DoomCtrl: Waveshare timeout -- searching again");
    }

    /* Send input at 20 Hz once connected */
    if (connected && now - lastSent > SEND_INTERVAL) {
        sendInput(joy1V, joy1H, joy2H, btnA, btnB);
        lastSent = now;
    }
}

void DoomCtrl::draw() {
    if (!running) return;
    display->clearDisplay();

    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(20, 0);
    display->print("DOOM CONTROLLER");
    display->drawLine(0, 10, 127, 10, SSD1306_WHITE);

    if (!connected) {
        display->setCursor(10, 18);
        display->print("Searching for");
        display->setCursor(10, 28);
        display->print("Waveshare display...");
        /* animated dots */
        unsigned long dots = (millis() / 400) % 4;
        for (unsigned long i = 0; i < dots; i++) {
            display->fillCircle(20 + (int)i * 12, 44, 3, SSD1306_WHITE);
        }
        display->setCursor(10, 56);
        display->print("BtnA: Cancel");
    } else {
        display->setCursor(10, 18);
        display->print("Connected!");
        display->setCursor(0, 32);
        display->print("Joy1: Move/Strafe");
        display->setCursor(0, 42);
        display->print("Joy2H: Rotate");
        display->setCursor(0, 52);
        display->print("BtnB:Shoot BtnA:Quit");
    }

    display->display();
}

bool DoomCtrl::isRunning()          { return running; }
bool DoomCtrl::shouldReturnToMenu() { return !running; }
