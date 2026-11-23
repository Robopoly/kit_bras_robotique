#ifndef PACMAN_GAME_H
#define PACMAN_GAME_H

#include "../common/config.h"

class PacManGame {
private:
  // Taille de la grille du labyrinthe (ajustée pour écran 128x64)
  static const int MAZE_WIDTH = 21;
  static const int MAZE_HEIGHT = 11;  // Réduit de 13 à 11
  static const int CELL_SIZE = 5;     // Réduit de 6 à 5 pixels
  
  // Décalage pour centrer le labyrinthe
  static const int MAZE_OFFSET_X = 1; // Réduit pour économiser l'espace
  static const int MAZE_OFFSET_Y = 2; // Réduit pour économiser l'espace
  
  // Types de cellules
  enum CellType {
    EMPTY = 0,
    WALL = 1,
    PELLET = 2,
    POWER_PELLET = 3,
    GHOST_SPAWN = 4
  };
  
  // Directions
  enum Direction {
    NONE = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4
  };
  
  // Structure pour Pac-Man
  struct Player {
    int x, y;           // Position dans la grille
    Direction dir;      // Direction actuelle
    Direction nextDir;  // Prochaine direction demandée
    int animFrame;      // Frame d'animation
    bool alive;
  };
  
  // Structure pour les fantômes
  struct Ghost {
    int x, y;           // Position dans la grille
    Direction dir;      // Direction actuelle
    int color;          // Couleur du fantôme (pour différenciation)
    bool active;
    bool frightened;    // Mode effrayé (vulnérable)
    unsigned long lastMove;
  };
  
  // Variables du jeu
  Player pacman;
  Ghost ghosts[3];    // 3 fantômes pour commencer
  
  // Carte du labyrinthe
  CellType maze[MAZE_HEIGHT][MAZE_WIDTH];
  
  // Variables de gameplay
  int score;
  int lives;
  int pelletsRemaining;
  bool gameOver;
  bool gameWon;
  unsigned long gameOverTime;
  
  // Contrôles et timing
  unsigned long lastPacmanMove;
  unsigned long lastGhostMove;
  static const unsigned long PACMAN_MOVE_DELAY = 150; // ms
  static const unsigned long GHOST_MOVE_DELAY = 250;  // ms (augmenté pour les ralentir)
  static const unsigned long FRIGHTENED_DURATION = 5000; // 5 secondes de mode effrayé
  
  // Mode frightened
  bool frightenedMode;
  unsigned long frightenedStartTime;
  unsigned long lastBlinkTime;
  bool blinkState;
  
  // Calibration joystick
  bool calibrated;
  int joystickCenter;
  
  // Retour au menu
  unsigned long lastMenuButtonTime;
  bool lastMenuBtn;
  static const unsigned long MENU_BUTTON_DEBOUNCE = 500;
  bool requestReturnToMenu;
  
  // Redémarrage
  unsigned long lastRestartButtonTime;
  bool lastRestartBtn;
  
  // Fonctions privées
  void initMaze();
  void initPlayer();
  void initGhosts();
  void updatePlayer();
  void updateGhosts();
  void checkCollisions();
  void drawMaze();
  void drawPlayer();
  void drawGhosts();
  void drawUI();
  bool canMoveTo(int x, int y);
  bool isValidDirection(int x, int y, Direction dir);
  void collectPellet(int x, int y);
  Direction getOppositeDirection(Direction dir);
  void resetGhost(int ghostIndex);
  
public:
  PacManGame();
  void init();
  void update();
  void draw();
  bool shouldReturnToMenu();
};

#endif