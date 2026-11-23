#ifndef DOOM_GAME_H
#define DOOM_GAME_H

#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include "../robot/robot_comm.h"

enum DoomMode {
    DOOM_MODE_SOLO       = 0,
    DOOM_MODE_DEATHMATCH = 1
};

class DoomGame {
private:
    Adafruit_SSD1306* display;

    // --- Mode ---
    DoomMode doomMode;

    // --- Position et orientation du joueur ---
    float playerX;
    float playerY;
    float playerAngle;

    // --- Map 2D du niveau (0 = vide, 1 = mur) ---
    static const int MAP_WIDTH  = 8;
    static const int MAP_HEIGHT = 8;
    int map[MAP_HEIGHT][MAP_WIDTH];

    // --- Ennemis (solo uniquement) ---
    struct Enemy {
        float x, y;
        bool alive;
        int health;
    };
    static const int MAX_ENEMIES = 10;
    Enemy enemies[MAX_ENEMIES];
    int enemiesAlive;
    int totalKills;

    // --- Système de niveaux (solo) ---
    int currentLevel;
    int totalLevelsCompleted;

    // --- Arme et tir ---
    bool isShooting;
    unsigned long shootStartTime;
    static const int SHOOT_ANIMATION_DURATION = 200; // ms
    bool lastButtonAState;

    // --- Objectif solo ---
    float exitX, exitY;
    bool levelComplete;

    // --- Constantes de jeu ---
    static constexpr float MOVE_SPEED = 0.15f;
    static constexpr float ROT_SPEED  = 0.08f;
    static constexpr float FOV        = 1.0f;
    static const int NUM_RAYS = 64;

    // --- État du jeu ---
    int  gameState;  // 0=mode_select, 1=waiting_peer, 2=playing, 3=game_over
    bool gameRunning;
    unsigned long lastUpdateTime;
    unsigned long gameStartTime;
    bool lastButtonBState;

    // ---- Menu mode de jeu ----
    int  menuSelection;
    bool lastMenuBtnState;
    unsigned long lastMenuBtnTime;
    int  lastJoy1VRaw;  // pour navigation menu

    // ---- ESP-NOW (deathmatch) ----
    bool espNowInitialized;
    bool peerConnected;
    uint8_t peerMAC[6];
    unsigned long lastPeerMessage;
    unsigned long lastStateSent;
    unsigned long lastDiscoverySent;

    // État de l'adversaire (reçu via ESP-NOW)
    float opponentX;
    float opponentY;
    float opponentAngle;
    int   opponentHealth;

    // Santé en deathmatch
    int myHealth;
    int myKills;
    int opponentKills;

    // Buffer de réception
    DoomPlayerState receivedState;
    DoomShootMsg    receivedShoot;
    bool newStateReceived;
    bool newShootReceived;
    bool opponentShooting;
    unsigned long opponentShootTime;

    // Intervalles
    static const unsigned long PEER_TIMEOUT        = 3000;
    static const unsigned long STATE_SEND_INTERVAL = 50;   // ~20 FPS
    static const unsigned long DISCOVERY_INTERVAL  = 500;

    // Deathmatch
    static const int DEATHMATCH_HP    = 3;  // PV de départ
    static const int KILLS_TO_WIN     = 3;  // Kills pour gagner

    // --- Méthodes privées solo ---
    void initMap();
    void generateRandomMap();
    void ensureMapConnectivity();
    void spawnEnemies();
    void nextLevel();
    float castRay(float angle, bool checkEnemies, int* hitEnemyIndex);
    void drawWalls();
    void drawMiniMap();
    void drawWeapon();
    void drawHUD();
    void handleInput(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB);
    void shoot();
    void checkVictory();

    // --- Méthodes privées multiplayer ---
    void initESPNow();
    void deinitESPNow();
    void addPeer(const uint8_t* mac);
    void removePeer(const uint8_t* mac);
    void sendPlayerState();
    void sendShootMsg();
    void sendHitMsg(uint8_t damage, uint8_t remainingHP);
    void sendDiscovery();

    void updateModeMenu(int joy1V, bool buttonA, bool buttonB);
    void drawModeMenu();
    void updateWaiting(bool buttonA);
    void drawWaiting();
    void updateDeathmatch(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB);
    void drawDeathmatch();

    // --- ESP-NOW callbacks ---
    static void onDataSentStatic(const uint8_t* mac, esp_now_send_status_t status);
    static void onDataReceivedStatic(const uint8_t* mac, const uint8_t* data, int len);

public:
    static DoomGame* instance;  // Singleton pour les callbacks ESP-NOW

    DoomGame(Adafruit_SSD1306* disp);
    void init();
    void start();
    void update(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB);
    void draw();
    bool isRunning();
    bool shouldReturnToMenu();

    // Appelées depuis les callbacks statiques
    void onDataSent(const uint8_t* mac, esp_now_send_status_t status);
    void onDataReceived(const uint8_t* mac, const uint8_t* data, int len);
};

#endif
