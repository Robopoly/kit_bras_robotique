#include "snake_game.h"

SnakeGame::SnakeGame() {
  init();
}

void SnakeGame::init() {
  // Initialisation des variables de jeu
  snakeLength = INITIAL_LENGTH;
  currentDirection = RIGHT;
  nextDirection = RIGHT;
  score = 0;
  gameOver = false;
  gameWon = false;
  
  // Timing
  lastMoveTime = millis();
  lastInput = 0;
  lastMenuButtonTime = millis();
  lastMenuBtn = HIGH;
  calibrated = false;
  
  // Initialiser le serpent et la nourriture
  initSnake();
  generateFood();
}

void SnakeGame::initSnake() {
  // Placer le serpent au centre de la grille
  int startX = GRID_WIDTH / 2;
  int startY = GRID_HEIGHT / 2;
  
  for (int i = 0; i < snakeLength; i++) {
    snake[i].x = startX - i;
    snake[i].y = startY;
  }
}

void SnakeGame::generateFood() {
  int attempts = 0;
  do {
    food.x = random(0, GRID_WIDTH);
    food.y = random(0, GRID_HEIGHT);
    attempts++;
  } while (isSnakeSegment(food.x, food.y) && attempts < 100);
  
  food.active = true;
}

void SnakeGame::update() {
  if (gameOver || gameWon) {
    // Vérifier le bouton de retour au menu
    bool menuBtn = digitalRead(JOY1_BTN_PIN);
    if (menuBtn == LOW && lastMenuBtn == HIGH) {
      if (millis() - lastMenuButtonTime > BUTTON_DEBOUNCE) {
        reset();
        return;
      }
    }
    lastMenuBtn = menuBtn;
    return;
  }
  
  updateDirection();
  
  unsigned long currentTime = millis();
  
  // Déplacer le serpent selon l'intervalle défini
  if (currentTime - lastMoveTime > MOVE_DELAY) {
    moveSnake();
    checkCollisions();
    lastMoveTime = currentTime;
  }
}

void SnakeGame::updateDirection() {
  unsigned long currentTime = millis();
  if (currentTime - lastInput < INPUT_DELAY) return;
  
  // Lecture du joystick
  int joystickY = analogRead(JOYSTICK_Y_PIN);
  int joystickH = analogRead(JOY1_H_PIN);
  
  // Calibration automatique
  if (!calibrated) {
    joystickCenter = joystickY;
    calibrated = true;
  }
  
  // Déterminer la nouvelle direction (avec inversions comme corrigées)
  Direction newDirection = NONE;
  
  if (joystickY < (joystickCenter - JOYSTICK_DEADZONE)) {
    newDirection = DOWN; // Joystick haut = serpent vers le bas
  } else if (joystickY > (joystickCenter + JOYSTICK_DEADZONE)) {
    newDirection = UP;   // Joystick bas = serpent vers le haut
  } else if (joystickH < (joystickCenter - JOYSTICK_DEADZONE)) {
    newDirection = RIGHT; // Joystick gauche = serpent vers la droite
  } else if (joystickH > (joystickCenter + JOYSTICK_DEADZONE)) {
    newDirection = LEFT;  // Joystick droite = serpent vers la gauche
  }
  
  // Empêcher le serpent de faire demi-tour
  if (newDirection != NONE) {
    if ((currentDirection == UP && newDirection != DOWN) ||
        (currentDirection == DOWN && newDirection != UP) ||
        (currentDirection == LEFT && newDirection != RIGHT) ||
        (currentDirection == RIGHT && newDirection != LEFT)) {
      nextDirection = newDirection;
      lastInput = currentTime;
    }
  }
}

void SnakeGame::moveSnake() {
  // Changer de direction si demandé
  if (nextDirection != NONE) {
    currentDirection = nextDirection;
    nextDirection = NONE;
  }
  
  // Calculer la nouvelle position de la tête
  int newHeadX = snake[0].x;
  int newHeadY = snake[0].y;
  
  switch (currentDirection) {
    case UP: newHeadY--; break;
    case DOWN: newHeadY++; break;
    case LEFT: newHeadX--; break;
    case RIGHT: newHeadX++; break;
    default: return;
  }
  
  // Vérifier les limites de la grille (game over si sortie)
  if (newHeadX < 0 || newHeadX >= GRID_WIDTH || 
      newHeadY < 0 || newHeadY >= GRID_HEIGHT) {
    gameOver = true;
    return;
  }
  
  // Vérifier collision avec le corps
  if (isSnakeSegment(newHeadX, newHeadY)) {
    gameOver = true;
    return;
  }
  
  // Déplacer le corps (de la queue vers la tête)
  bool ateFood = (newHeadX == food.x && newHeadY == food.y && food.active);
  
  if (!ateFood) {
    // Déplacer normalement (supprimer la queue)
    for (int i = snakeLength - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
  } else {
    // Manger la nourriture (grandir)
    for (int i = snakeLength; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    snakeLength++;
    score += 10;
    
    // Vérifier victoire (serpent remplit presque toute la grille)
    if (snakeLength >= GRID_WIDTH * GRID_HEIGHT - 5) {
      gameWon = true;
      return;
    }
    
    generateFood();
  }
  
  // Ajouter la nouvelle tête
  snake[0].x = newHeadX;
  snake[0].y = newHeadY;
}

void SnakeGame::checkCollisions() {
  // Les collisions sont déjà gérées dans moveSnake()
}

bool SnakeGame::isSnakeSegment(int x, int y) {
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x == x && snake[i].y == y) {
      return true;
    }
  }
  return false;
}

void SnakeGame::draw() {
  display.clearDisplay();
  
  drawScore();
  drawGrid();
  drawSnake();
  drawFood();
  
  if (gameOver) {
    drawGameOver();
  } else if (gameWon) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(35, 30);
    display.println("YOU WIN!");
    display.setCursor(25, 45);
    display.println("BTN: Menu");
  }
  
  display.display();
}

void SnakeGame::drawScore() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Score: ");
  display.print(score);
  display.setCursor(80, 0);
  display.print("Len: ");
  display.print(snakeLength);
}

void SnakeGame::drawGrid() {
  // Dessiner le contour de la grille
  int gridX = GRID_OFFSET_X;
  int gridY = GRID_OFFSET_Y;
  int gridPixelWidth = GRID_WIDTH * CELL_SIZE;
  int gridPixelHeight = GRID_HEIGHT * CELL_SIZE;
  
  display.drawRect(gridX - 1, gridY - 1, gridPixelWidth + 2, gridPixelHeight + 2, SSD1306_WHITE);
}

void SnakeGame::drawSnake() {
  for (int i = 0; i < snakeLength; i++) {
    int pixelX = GRID_OFFSET_X + snake[i].x * CELL_SIZE;
    int pixelY = GRID_OFFSET_Y + snake[i].y * CELL_SIZE;
    
    if (i == 0) {
      // Tête du serpent (pleine)
      display.fillRect(pixelX, pixelY, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
    } else {
      // Corps du serpent (contour)
      display.drawRect(pixelX, pixelY, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
    }
  }
}

void SnakeGame::drawFood() {
  if (food.active) {
    int pixelX = GRID_OFFSET_X + food.x * CELL_SIZE;
    int pixelY = GRID_OFFSET_Y + food.y * CELL_SIZE;
    
    // Dessiner la nourriture comme un cercle (approximé)
    display.fillRect(pixelX + 1, pixelY + 1, CELL_SIZE - 2, CELL_SIZE - 2, SSD1306_WHITE);
    display.drawPixel(pixelX + CELL_SIZE/2, pixelY, SSD1306_WHITE);
    display.drawPixel(pixelX, pixelY + CELL_SIZE/2, SSD1306_WHITE);
    display.drawPixel(pixelX + CELL_SIZE - 1, pixelY + CELL_SIZE/2, SSD1306_WHITE);
    display.drawPixel(pixelX + CELL_SIZE/2, pixelY + CELL_SIZE - 1, SSD1306_WHITE);
  }
}

void SnakeGame::drawGameOver() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 25);
  display.println("GAME OVER");
  display.setCursor(25, 35);
  display.print("Score: ");
  display.print(score);
  display.setCursor(25, 50);
  display.println("BTN: Menu");
}

bool SnakeGame::shouldReturnToMenu() {
  bool menuBtn = digitalRead(JOY1_BTN_PIN);
  if (menuBtn == LOW && lastMenuBtn == HIGH) {
    if (millis() - lastMenuButtonTime > BUTTON_DEBOUNCE) {
      lastMenuButtonTime = millis();
      return true;
    }
  }
  lastMenuBtn = menuBtn;
  return false;
}

void SnakeGame::reset() {
  init();
}

int SnakeGame::getScore() {
  return score;
}