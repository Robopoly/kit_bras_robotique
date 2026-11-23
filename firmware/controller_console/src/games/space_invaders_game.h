#ifndef SPACE_INVADERS_GAME_H
#define SPACE_INVADERS_GAME_H

#include "../common/config.h"

class SpaceInvadersGame {
private:
  // Paramètres du jeu
  static const int PLAYER_WIDTH = 8;
  static const int PLAYER_HEIGHT = 4;
  static const int INVADER_WIDTH = 6;
  static const int INVADER_HEIGHT = 4;
  static const int BULLET_WIDTH = 1;
  static const int BULLET_HEIGHT = 3;
  static const int MAX_BULLETS = 8; // Augmenté pour tir multiple
  static const int MAX_INVADER_BULLETS = 4; // Augmenté pour difficulté progressive
  static const int INVADERS_ROWS = 4;
  static const int INVADERS_COLS = 8;
  static const int MAX_POWERUPS = 3;
  static const int MAX_LIVES = 5; // Limite de vies
  
  // Types de bonus
  enum PowerUpType {
    TRIPLE_SHOT = 0,
    AUTO_FIRE = 1,
    EXTRA_LIFE = 2
  };
  
  // Structures
  struct Player {
    int x, y;
    bool alive;
    bool autoFire;
    bool tripleShot;
    unsigned long autoFireLastShot;
    unsigned long powerUpEndTime;
    PowerUpType activePowerUp;
  };
  
  struct Bullet {
    int x, y;
    bool active;
    bool fromPlayer;
  };
  
  struct Invader {
    int x, y;
    bool alive;
  };
  
  struct PowerUp {
    int x, y;
    bool active;
    PowerUpType type;
    unsigned long spawnTime;
  };
  
  // Variables du jeu
  Player player;
  Bullet bullets[MAX_BULLETS + MAX_INVADER_BULLETS];
  Invader invaders[INVADERS_ROWS][INVADERS_COLS];
  PowerUp powerUps[MAX_POWERUPS];
  int invaderDirection;
  unsigned long lastInvaderMove;
  unsigned long lastInvaderShoot;
  int score;
  int lives;
  int waveNumber;           // Numéro de la vague actuelle
  int invaderShootDelay;    // Délai entre tirs ennemis (diminue avec difficulté)
  bool gameOver;
  bool calibrated;
  
  // Debouncing pour retour menu
  unsigned long lastMenuButtonTime;
  bool lastMenuBtn;
  static const unsigned long MENU_BUTTON_DEBOUNCE = 500; // 500ms debouncing
  bool requestReturnToMenu;
  
  // Fonctions privées
  void initGame();
  void initWave();
  void updatePlayer();
  void updateBullets();
  void updateInvaders();
  void updatePowerUps();
  void checkCollisions();
  void drawPlayer();
  void drawBullets();
  void drawInvaders();
  void drawPowerUps();
  void drawUI();
  void fireBullet(int x, int y, bool fromPlayer);
  void fireTripleShot(int x, int y);
  void invaderShoot();
  void spawnPowerUp(int x, int y);
  void collectPowerUp(int index);
  void updatePlayerPowerUps();
  bool checkWaveComplete();
  
public:
  SpaceInvadersGame();
  void init();
  void update();
  void draw();
  bool shouldReturnToMenu();
};

#endif