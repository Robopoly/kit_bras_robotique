#ifndef ROBOT_COMM_H
#define ROBOT_COMM_H

#include <esp_now.h>
#include <WiFi.h>

// Structure de commande pour le bras robotique
struct RobotCommand {
  uint8_t servo1;      // Angle servo base (0-180°)
  uint8_t servo2;      // Angle servo épaule (0-180°) 
  uint8_t servo3;      // Angle servo coude (0-180°)
  uint8_t servo4;      // Angle servo poignet (0-180°)
  uint8_t servo5;      // Angle servo pince (0-180°)
  uint8_t mode;        // Mode de contrôle (0=manuel, 1=auto, etc.)
  uint8_t speed;       // Vitesse de mouvement (0-100%)
  uint32_t timestamp;  // Horodatage pour éviter les doublons
  uint32_t checksum;   // Vérification intégrité
};

// Structure de feedback du bras vers la manette
struct RobotStatus {
  uint8_t battery_level;    // Niveau batterie (0-100%)
  uint8_t connection_rssi;  // Force signal WiFi
  uint8_t servo_positions[5]; // Positions actuelles des servos
  bool emergency_stop;      // État d'arrêt d'urgence
  uint32_t timestamp;       // Horodatage
  uint32_t checksum;        // Vérification intégrité
};

// Adresses MAC (à modifier selon vos ESP32)
extern uint8_t robotArmMAC[6];      // MAC du bras robotique
extern uint8_t controllerMAC[6];    // MAC de la manette

// ============================================
// PONG MULTIJOUEUR - Structures de communication
// ============================================

// Types de messages Pong
enum PongMessageType : uint8_t {
  PONG_MSG_DISCOVERY = 0x10,     // Recherche de partenaire
  PONG_MSG_DISCOVERY_ACK = 0x11, // Réponse à la recherche
  PONG_MSG_GAME_STATE = 0x20,    // État complet du jeu (Host → Client)
  PONG_MSG_PADDLE_POS = 0x30,    // Position paddle (Client → Host)
  PONG_MSG_GAME_START = 0x40,    // Démarrer la partie
  PONG_MSG_GAME_END = 0x50,      // Fin de partie
  PONG_MSG_PING = 0x60,          // Ping pour vérifier connexion
  PONG_MSG_PONG = 0x61           // Réponse au ping
};

// Message de découverte (broadcast)
struct PongDiscoveryMsg {
  uint8_t msgType;          // PONG_MSG_DISCOVERY ou PONG_MSG_DISCOVERY_ACK
  uint8_t isHost;           // 1 si c'est un host qui cherche un client
  uint8_t playerName[8];    // Nom du joueur (optionnel)
  uint32_t timestamp;
};

// État complet du jeu envoyé par le Host au Client
struct PongGameState {
  uint8_t msgType;          // PONG_MSG_GAME_STATE
  // Position de la balle
  int16_t ballX;            // Position X de la balle (x100 pour précision)
  int16_t ballY;            // Position Y de la balle (x100 pour précision)
  int16_t ballDX;           // Direction X de la balle (x100)
  int16_t ballDY;           // Direction Y de la balle (x100)
  // Positions des raquettes
  uint8_t paddle1Y;         // Position Y paddle gauche (Host)
  uint8_t paddle2Y;         // Position Y paddle droite (Client)
  // Scores
  uint8_t score1;           // Score joueur 1 (Host)
  uint8_t score2;           // Score joueur 2 (Client)
  // État du jeu
  uint8_t gameState;        // 0=attente, 1=en cours, 2=pause, 3=terminé
  uint8_t maxPoints;        // Points pour gagner
  uint32_t timestamp;
};

// Position du paddle envoyée par le Client au Host
struct PongPaddleMsg {
  uint8_t msgType;          // PONG_MSG_PADDLE_POS
  uint8_t paddleY;          // Position Y du paddle
  uint8_t buttonPressed;    // État des boutons (pour pause, etc.)
  uint32_t timestamp;
};

// Message de contrôle (start, end, ping)
struct PongControlMsg {
  uint8_t msgType;
  uint8_t data;             // Données additionnelles
  uint32_t timestamp;
};

// ============================================
// DOOM MULTIJOUEUR - Structures de communication
// ============================================

enum DoomMessageType : uint8_t {
  DOOM_MSG_DISCOVERY     = 0xA0,  // Broadcast: recherche d'un partenaire
  DOOM_MSG_DISCOVERY_ACK = 0xA1,  // Réponse à la découverte
  DOOM_MSG_PLAYER_STATE  = 0xB0,  // État du joueur (position + angle) bidirectionnel
  DOOM_MSG_SHOOT         = 0xB1,  // Notification de tir
  DOOM_MSG_HIT           = 0xB2,  // Confirmation de touche
  DOOM_MSG_GAME_OVER     = 0xB3   // Fin de partie
};

// Envoyé par chaque joueur ~20 FPS : position + angle
struct DoomPlayerState {
  uint8_t  msgType;      // DOOM_MSG_PLAYER_STATE
  int16_t  posX;         // Position X × 100
  int16_t  posY;         // Position Y × 100
  int16_t  angle;        // Angle × 1000 (radians)
  uint8_t  health;       // Points de vie (0-100)
  uint8_t  isShooting;   // 1 si en train de tirer cette frame
  uint32_t timestamp;
};

// Envoyé lors d'un tir pour déclencher côté adverse la vérification de touche
struct DoomShootMsg {
  uint8_t  msgType;      // DOOM_MSG_SHOOT
  int16_t  originX;      // Position X tireur × 100
  int16_t  originY;      // Position Y tireur × 100
  int16_t  angle;        // Angle du tir × 1000
  uint32_t timestamp;
};

// Envoyé quand un joueur est touché (calculé localement par la victime)
struct DoomHitMsg {
  uint8_t  msgType;      // DOOM_MSG_HIT
  uint8_t  damage;       // Dégâts subis
  uint8_t  remainingHP;  // PV restants
  uint32_t timestamp;
};

// Adresse broadcast pour découverte
extern const uint8_t BROADCAST_MAC[6];

// Fonctions utilitaires
uint32_t calculateChecksum(const void* data, size_t size);
bool validateCommand(const RobotCommand& cmd);
bool validateStatus(const RobotStatus& status);

#endif