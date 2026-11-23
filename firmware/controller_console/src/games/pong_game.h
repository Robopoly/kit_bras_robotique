#ifndef PONG_GAME_H
#define PONG_GAME_H

#include "../common/config.h"
#include "../robot/robot_comm.h"
#include <esp_now.h>
#include <WiFi.h>

// Modes de jeu Pong
enum PongMode {
  PONG_MODE_LOCAL = 0,      // 2 joueurs sur la même manette
  PONG_MODE_HOST = 1,       // Multijoueur - cette manette est Host
  PONG_MODE_CLIENT = 2      // Multijoueur - cette manette est Client
};

class PongGame {
private:
  // Paramètres du jeu
  static const int PADDLE_HEIGHT = 12;
  static const int PADDLE_WIDTH = 2;
  static const int BALL_SIZE = 2;
  
  // Raquettes
  int paddle1_y;
  int paddle2_y;
  
  // Balle
  float ball_xf, ball_yf;
  float ball_dx, ball_dy;
  float ball_speed;
  float base_ball_speed; // Vitesse de base pour reset
  int ball_x, ball_y;
  
  // Timer pour augmentation vitesse
  unsigned long gameStartTime;
  unsigned long lastSpeedIncreaseTime;
  
  // Scores
  int score1, score2;
  int maxPoints;
  
  // États du jeu
  int gameState; // 0=MENU, 1=PLAY, 2=GAME_OVER, 3=MULTIPLAYER_MENU, 4=WAITING_CONNECTION
  int menuIndex;
  bool lastJoy2Btn;
  bool editingMaxPoints;
  unsigned long blinkTimer;
  bool blinkState;
  
  // Debouncing pour retour menu
  unsigned long lastMenuButtonTime;
  static const unsigned long MENU_BUTTON_DEBOUNCE = 500; // 500ms debouncing
  bool requestReturnToMenu;
  bool lastJoy1Btn; // Suivi état précédent GPIO5 pour détection de transition
  
  // ============================================
  // MULTIJOUEUR
  // ============================================
  PongMode pongMode;                    // Mode de jeu actuel
  bool espNowInitialized;               // ESP-NOW initialisé ?
  bool peerConnected;                   // Autre manette connectée ?
  uint8_t peerMAC[6];                   // MAC de l'autre manette
  unsigned long lastPeerMessage;        // Dernier message reçu du peer
  unsigned long lastStateSent;          // Dernier état envoyé
  static const unsigned long PEER_TIMEOUT = 2000;       // Timeout connexion (2s)
  static const unsigned long STATE_SEND_INTERVAL = 33;  // ~30 FPS pour envoi état
  static const unsigned long DISCOVERY_INTERVAL = 500;  // Intervalle découverte (500ms)
  unsigned long lastDiscoverySent;      // Dernier message de découverte
  
  // Buffer pour réception
  PongGameState receivedGameState;
  PongPaddleMsg receivedPaddleMsg;
  bool newGameStateReceived;
  bool newPaddleMsgReceived;

  // ============================================
  // HAPTIC FEEDBACK (inverse demande utilisateur: gauche=GPIO18, droite=GPIO17)
  // ============================================
  static const int MOTOR_LEFT_PIN = 18;
  static const int MOTOR_RIGHT_PIN = 17;
  static const unsigned long HAPTIC_PULSE_MS = 60;
  bool motorsInitialized;
  unsigned long motorLeftOffTime;
  unsigned long motorRightOffTime;
  
  // Fonctions privées - Jeu
  void resetBall();
  void updatePaddles();
  void updateBall();
  void checkCollisions();
  void drawMenu();
  void drawGame();
  void drawGameOver(int winner);
  
  // Fonctions privées - Multijoueur
  void drawMultiplayerMenu();
  void drawWaitingConnection();
  void updateMultiplayerMenu();
  void updateWaitingConnection();
  void updateAsHost();
  void updateAsClient();
  void initESPNow();
  void deinitESPNow();
  void sendDiscovery(bool isHost);
  void sendGameState();
  void sendPaddlePosition();
  void addPeer(const uint8_t* mac);
  void removePeer();
  void initHaptics();
  void updateHaptics();
  void triggerLeftHaptic();
  void triggerRightHaptic();
  
  // Callbacks ESP-NOW (statiques)
  static void onDataSentStatic(const uint8_t *mac_addr, esp_now_send_status_t status);
  static void onDataReceivedStatic(const uint8_t *mac, const uint8_t *data, int len);
  
public:
  PongGame();
  void init();
  void update();
  void draw();
  bool shouldReturnToMenu();
  
  // Handlers pour callbacks (appelés par les fonctions statiques)
  void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
  void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);
  
  // Instance statique pour callbacks
  static PongGame* instance;
};

#endif