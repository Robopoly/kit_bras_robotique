#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include "../common/config.h"

class SnakeGame {
private:
  // Dimensions de la grille (adaptées pour écran 128x64)
  static const int GRID_WIDTH = 25;   // 25*5 = 125 pixels (+ bordures = 127)
  static const int GRID_HEIGHT = 11;  // 11*5 = 55 pixels (+ offset 4 + bordures = 61)
  static const int CELL_SIZE = 5;     // Taille des cellules
  static const int GRID_OFFSET_X = 1; // Décalage horizontal
  static const int GRID_OFFSET_Y = 4; // Décalage vertical pour laisser place au score
  
  // Paramètres de gameplay
  static const int MAX_SNAKE_LENGTH = 100;
  static const int MOVE_DELAY = 200; // ms entre les mouvements
  static const int INITIAL_LENGTH = 3;
  
  // Structure pour les segments du serpent
  struct SnakeSegment {
    int x;
    int y;
  };
  
  // Structure pour la nourriture
  struct Food {
    int x;
    int y;
    bool active;
  };
  
  // Directions possibles
  enum Direction {
    NONE = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4
  };
  
  // Variables du jeu
  SnakeSegment snake[MAX_SNAKE_LENGTH];
  int snakeLength;
  Direction currentDirection;
  Direction nextDirection;
  Food food;
  int score;
  bool gameOver;
  bool gameWon;
  
  // === Constantes de timing ===
  static const int INPUT_DELAY = 200;           // ms entre les inputs
  static const unsigned long BUTTON_DEBOUNCE = 300;    // 300ms debouncing pour menus
  static const unsigned long MENU_BUTTON_DEBOUNCE = 500; // 500ms debouncing pour boutons de retour menu
  
  // Variables de timing
  unsigned long lastMoveTime;
  unsigned long lastInput;
  
  // Variables de contrôle
  unsigned long lastMenuButtonTime;
  bool lastMenuBtn;
  bool calibrated;
  int joystickCenter;
  
  // Méthodes privées
  void initSnake();
  void generateFood();
  void moveSnake();
  void checkCollisions();
  void updateDirection();
  void drawGrid();
  void drawSnake();
  void drawFood();
  void drawScore();
  void drawGameOver();
  bool isValidPosition(int x, int y);
  bool isSnakeSegment(int x, int y);
  
public:
  SnakeGame();
  void init();
  void update();
  void draw();
  bool shouldReturnToMenu();
  void reset();
  int getScore();
};

#endif