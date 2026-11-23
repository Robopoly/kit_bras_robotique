#include "pacman_game.h"

PacManGame::PacManGame() :
  score(0), lives(3), pelletsRemaining(0), gameOver(false), gameWon(false), gameOverTime(0),
  lastPacmanMove(0), lastGhostMove(0), calibrated(false), joystickCenter(2048),
  lastMenuButtonTime(0), lastMenuBtn(HIGH), requestReturnToMenu(false),
  lastRestartButtonTime(0), lastRestartBtn(HIGH),
  frightenedMode(false), frightenedStartTime(0), lastBlinkTime(0), blinkState(false) {}

void PacManGame::init() {
  score = 0;
  lives = 3;
  pelletsRemaining = 0;
  gameOver = false;
  gameWon = false;
  gameOverTime = 0;
  requestReturnToMenu = false;
  lastMenuButtonTime = millis();
  lastMenuBtn = HIGH;
  lastRestartButtonTime = millis();
  lastRestartBtn = HIGH;
  calibrated = false;
  frightenedMode = false;
  frightenedStartTime = 0;
  lastBlinkTime = 0;
  blinkState = false;
  
  initMaze();
  initPlayer();
  initGhosts();
}

void PacManGame::initMaze() {
  // Labyrinthe simplifié adapté à l'écran 128x64 (11 lignes)
  CellType mazePattern[MAZE_HEIGHT][MAZE_WIDTH] = {
    {WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL},
    {WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL},
    {WALL,POWER_PELLET,WALL,WALL,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,WALL,WALL,POWER_PELLET,WALL},
    {WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL},
    {WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,PELLET,WALL,EMPTY,EMPTY,EMPTY,WALL,PELLET,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL},
    {WALL,WALL,WALL,WALL,WALL,PELLET,WALL,EMPTY,EMPTY,EMPTY,WALL,EMPTY,EMPTY,EMPTY,WALL,PELLET,WALL,WALL,WALL,WALL,WALL},
    {WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,PELLET,WALL,EMPTY,EMPTY,EMPTY,WALL,PELLET,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL},
    {WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL},
    {WALL,POWER_PELLET,WALL,WALL,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,PELLET,WALL,WALL,WALL,PELLET,WALL,WALL,WALL,POWER_PELLET,WALL},
    {WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,PELLET,WALL},
    {WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL,WALL}
  };
  
  pelletsRemaining = 0;
  
  for (int y = 0; y < MAZE_HEIGHT; y++) {
    for (int x = 0; x < MAZE_WIDTH; x++) {
      maze[y][x] = mazePattern[y][x];
      if (maze[y][x] == PELLET || maze[y][x] == POWER_PELLET) {
        pelletsRemaining++;
      }
    }
  }
}

void PacManGame::initPlayer() {
  pacman.x = 10;  // Centre du labyrinthe
  pacman.y = 7;   // Ajusté pour labyrinthe 11 lignes (était 9)
  pacman.dir = NONE;
  pacman.nextDir = NONE;
  pacman.animFrame = 0;
  pacman.alive = true;
}

void PacManGame::initGhosts() {
  // Fantôme 1 - Rouge
  ghosts[0].x = 9;
  ghosts[0].y = 5;  // Ajusté pour labyrinthe 11 lignes (était 6)
  ghosts[0].dir = LEFT;
  ghosts[0].color = 1;
  ghosts[0].active = true;
  ghosts[0].frightened = false;
  ghosts[0].lastMove = 0;
  
  // Fantôme 2 - Blanc
  ghosts[1].x = 10;
  ghosts[1].y = 5;  // Ajusté pour labyrinthe 11 lignes (était 6)
  ghosts[1].dir = UP;
  ghosts[1].color = 2;
  ghosts[1].active = true;
  ghosts[1].frightened = false;
  ghosts[1].lastMove = 0;
  
  // Fantôme 3 - Gris
  ghosts[2].x = 11;
  ghosts[2].y = 5;  // Ajusté pour labyrinthe 11 lignes (était 6)
  ghosts[2].dir = RIGHT;
  ghosts[2].color = 3;
  ghosts[2].active = true;
  ghosts[2].frightened = false;
  ghosts[2].lastMove = 0;
}

void PacManGame::update() {
  // Vérifier retour au menu
  bool menuBtn = digitalRead(JOY1_BTN);
  unsigned long currentTime = millis();
  
  if (menuBtn == LOW && lastMenuBtn == HIGH && (currentTime - lastMenuButtonTime > MENU_BUTTON_DEBOUNCE)) {
    requestReturnToMenu = true;
    lastMenuButtonTime = currentTime;
    return;
  }
  lastMenuBtn = menuBtn;
  
  // Si la partie est terminée, vérifier si on veut recommencer avec JOY2_SW
  if (gameOver || gameWon) {
    bool restartBtn = digitalRead(JOY2_BTN);
    
    // Attendre au moins 1 seconde après la fin de partie avant de permettre le restart
    if (currentTime - gameOverTime > 1000) {
      // Détecter uniquement un nouvel appui (transition HIGH -> LOW)
      if (restartBtn == LOW && lastRestartBtn == HIGH && (currentTime - lastRestartButtonTime > MENU_BUTTON_DEBOUNCE)) {
        // Réinitialiser le jeu
        lastRestartButtonTime = currentTime;
        init();
        return;
      }
    }
    
    // Toujours mettre à jour l'état précédent du bouton
    lastRestartBtn = restartBtn;
  }
  
  // Gérer le clignotement pour power pellets et fantômes effrayés
  if (currentTime - lastBlinkTime > 200) {  // Clignoter toutes les 200ms
    blinkState = !blinkState;
    lastBlinkTime = currentTime;
  }
  
  // Vérifier fin du mode frightened
  if (frightenedMode && (currentTime - frightenedStartTime > FRIGHTENED_DURATION)) {
    frightenedMode = false;
    for (int i = 0; i < 3; i++) {
      ghosts[i].frightened = false;
    }
  }
  
  if (!gameOver && !gameWon) {
    updatePlayer();
    updateGhosts();
    checkCollisions();
    
    // Vérifier victoire
    if (pelletsRemaining <= 0) {
      gameWon = true;
      gameOverTime = currentTime;
    }
  }
}

void PacManGame::updatePlayer() {
  // Lecture du joystick pour direction
  int joystickY = analogRead(JOYSTICK_Y_PIN);
  int joystickH = analogRead(JOY1_H_PIN);
  
  // Calibration automatique
  if (!calibrated) {
    joystickCenter = joystickY;
    calibrated = true;
  }
  
  // Déterminer la direction demandée
  if (joystickY < (joystickCenter - JOYSTICK_DEADZONE)) {
    pacman.nextDir = DOWN; // Inverser : joystick haut = direction bas à l'écran
  } else if (joystickY > (joystickCenter + JOYSTICK_DEADZONE)) {
    pacman.nextDir = UP;   // Inverser : joystick bas = direction haut à l'écran
  } else if (joystickH < (joystickCenter - JOYSTICK_DEADZONE)) {
    pacman.nextDir = RIGHT; // Inverser : joystick gauche = droite
  } else if (joystickH > (joystickCenter + JOYSTICK_DEADZONE)) {
    pacman.nextDir = LEFT;  // Inverser : joystick droite = gauche
  }
  
  unsigned long currentTime = millis();
  if (currentTime - lastPacmanMove > PACMAN_MOVE_DELAY) {
    // Essayer de changer de direction si possible
    if (pacman.nextDir != NONE && isValidDirection(pacman.x, pacman.y, pacman.nextDir)) {
      pacman.dir = pacman.nextDir;
      pacman.nextDir = NONE;
    }
    
    // Déplacer Pac-Man
    int newX = pacman.x;
    int newY = pacman.y;
    
    switch (pacman.dir) {
      case UP: newY--; break;
      case DOWN: newY++; break;
      case LEFT: newX--; break;
      case RIGHT: newX++; break;
      default: break;
    }
    
    if (canMoveTo(newX, newY)) {
      pacman.x = newX;
      pacman.y = newY;
      collectPellet(pacman.x, pacman.y);
    } else {
      pacman.dir = NONE; // Arrêter si collision
    }
    
    pacman.animFrame = (pacman.animFrame + 1) % 4;
    lastPacmanMove = currentTime;
  }
}

void PacManGame::updateGhosts() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < 3; i++) {
    if (!ghosts[i].active) continue;
    
    if (currentTime - ghosts[i].lastMove > GHOST_MOVE_DELAY) {
      // IA améliorée: poursuivre Pac-Man OU fuir si effrayé
      int deltaX = pacman.x - ghosts[i].x;
      int deltaY = pacman.y - ghosts[i].y;
      
      // Si le fantôme est effrayé, inverser les deltas pour fuir
      if (ghosts[i].frightened) {
        deltaX = -deltaX;
        deltaY = -deltaY;
      }
      
      // Créer une liste de directions prioritaires basée sur la position de Pac-Man
      Direction priorities[4];
      int priorityCount = 0;
      
      // Prioriser le mouvement horizontal si distance X > distance Y
      if (abs(deltaX) > abs(deltaY)) {
        // Prioriser horizontal
        if (deltaX > 0) priorities[priorityCount++] = RIGHT;
        else if (deltaX < 0) priorities[priorityCount++] = LEFT;
        
        if (deltaY > 0) priorities[priorityCount++] = DOWN;
        else if (deltaY < 0) priorities[priorityCount++] = UP;
      } else {
        // Prioriser vertical
        if (deltaY > 0) priorities[priorityCount++] = DOWN;
        else if (deltaY < 0) priorities[priorityCount++] = UP;
        
        if (deltaX > 0) priorities[priorityCount++] = RIGHT;
        else if (deltaX < 0) priorities[priorityCount++] = LEFT;
      }
      
      // Ajouter les directions restantes
      Direction allDirs[] = {UP, DOWN, LEFT, RIGHT};
      for (int j = 0; j < 4; j++) {
        bool alreadyInList = false;
        for (int k = 0; k < priorityCount; k++) {
          if (allDirs[j] == priorities[k]) {
            alreadyInList = true;
            break;
          }
        }
        if (!alreadyInList && priorityCount < 4) {
          priorities[priorityCount++] = allDirs[j];
        }
      }
      
      // Essayer chaque direction par ordre de priorité
      bool moved = false;
      for (int j = 0; j < priorityCount && !moved; j++) {
        if (isValidDirection(ghosts[i].x, ghosts[i].y, priorities[j])) {
          ghosts[i].dir = priorities[j];
          
          int newX = ghosts[i].x;
          int newY = ghosts[i].y;
          switch (ghosts[i].dir) {
            case UP: newY--; break;
            case DOWN: newY++; break;
            case LEFT: newX--; break;
            case RIGHT: newX++; break;
            default: break;
          }
          
          if (canMoveTo(newX, newY)) {
            ghosts[i].x = newX;
            ghosts[i].y = newY;
            moved = true;
          }
        }
      }
      
      ghosts[i].lastMove = currentTime;
    }
  }
}

void PacManGame::checkCollisions() {
  // Vérifier collision avec les fantômes
  for (int i = 0; i < 3; i++) {
    if (ghosts[i].active && ghosts[i].x == pacman.x && ghosts[i].y == pacman.y) {
      if (ghosts[i].frightened) {
        // Fantôme vulnérable : le manger !
        ghosts[i].active = false;
        score += 200;  // Bonus pour manger un fantôme
      } else {
        // Fantôme normal : perdre une vie
        lives--;
        if (lives <= 0) {
          gameOver = true;
          gameOverTime = millis();
        } else {
          // Repositionner Pac-Man
          initPlayer();
          delay(1000); // Pause avant de continuer
        }
      }
    }
  }
}

bool PacManGame::canMoveTo(int x, int y) {
  if (x < 0 || x >= MAZE_WIDTH || y < 0 || y >= MAZE_HEIGHT) {
    return false;
  }
  return maze[y][x] != WALL;
}

bool PacManGame::isValidDirection(int x, int y, Direction dir) {
  int newX = x, newY = y;
  switch (dir) {
    case UP: newY--; break;
    case DOWN: newY++; break;
    case LEFT: newX--; break;
    case RIGHT: newX++; break;
    default: return false;
  }
  return canMoveTo(newX, newY);
}

void PacManGame::collectPellet(int x, int y) {
  if (maze[y][x] == PELLET) {
    maze[y][x] = EMPTY;
    score += 10;
    pelletsRemaining--;
  } else if (maze[y][x] == POWER_PELLET) {
    maze[y][x] = EMPTY;
    score += 50;
    pelletsRemaining--;
    
    // Activer mode frightened
    frightenedMode = true;
    frightenedStartTime = millis();
    for (int i = 0; i < 3; i++) {
      ghosts[i].frightened = true;
      // Inverser direction des fantômes
      ghosts[i].dir = getOppositeDirection(ghosts[i].dir);
    }
  }
}

PacManGame::Direction PacManGame::getOppositeDirection(Direction dir) {
  switch (dir) {
    case UP: return DOWN;
    case DOWN: return UP;
    case LEFT: return RIGHT;
    case RIGHT: return LEFT;
    default: return NONE;
  }
}

void PacManGame::draw() {
  display.clearDisplay();
  
  if (gameOver || gameWon) {
    // Ne pas dessiner la map, juste le message de fin
    if (gameOver) {
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 15);
      display.print("GAME OVER");
      display.setTextSize(1);
      display.setCursor(25, 35);
      display.print("Score: ");
      display.print(score);
      display.setCursor(5, 50);
      display.print("JOY2: Restart");
    } else if (gameWon) {
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(20, 15);
      display.print("YOU WIN!");
      display.setTextSize(1);
      display.setCursor(25, 35);
      display.print("Score: ");
      display.print(score);
      display.setCursor(5, 50);
      display.print("JOY2: Restart");
    }
  } else {
    // Jeu en cours : dessiner la map et les éléments
    drawMaze();
    drawPlayer();
    drawGhosts();
    drawUI();
  }
  
  display.display();
}

void PacManGame::drawMaze() {
  for (int y = 0; y < MAZE_HEIGHT; y++) {
    for (int x = 0; x < MAZE_WIDTH; x++) {
      int screenX = MAZE_OFFSET_X + x * CELL_SIZE;
      int screenY = MAZE_OFFSET_Y + y * CELL_SIZE;
      
      switch (maze[y][x]) {
        case WALL:
          display.fillRect(screenX, screenY, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
          break;
        case PELLET:
          display.fillCircle(screenX + CELL_SIZE/2, screenY + CELL_SIZE/2, 1, SSD1306_WHITE);
          break;
        case POWER_PELLET:
          // Faire clignoter les power pellets
          if (blinkState) {
            display.fillCircle(screenX + CELL_SIZE/2, screenY + CELL_SIZE/2, 2, SSD1306_WHITE);
          }
          break;
      }
    }
  }
}

void PacManGame::drawPlayer() {
  if (!pacman.alive) return;
  
  int screenX = MAZE_OFFSET_X + pacman.x * CELL_SIZE + CELL_SIZE/2;
  int screenY = MAZE_OFFSET_Y + pacman.y * CELL_SIZE + CELL_SIZE/2;
  
  // Dessiner Pac-Man (cercle avec "bouche")
  display.fillCircle(screenX, screenY, 2, SSD1306_WHITE);
  
  // Animation de la bouche basée sur la direction
  switch (pacman.dir) {
    case RIGHT:
      if (pacman.animFrame < 2) {
        display.drawLine(screenX, screenY, screenX + 2, screenY - 1, SSD1306_BLACK);
        display.drawLine(screenX, screenY, screenX + 2, screenY + 1, SSD1306_BLACK);
      }
      break;
    case LEFT:
      if (pacman.animFrame < 2) {
        display.drawLine(screenX, screenY, screenX - 2, screenY - 1, SSD1306_BLACK);
        display.drawLine(screenX, screenY, screenX - 2, screenY + 1, SSD1306_BLACK);
      }
      break;
    case UP:
      if (pacman.animFrame < 2) {
        display.drawLine(screenX, screenY, screenX - 1, screenY - 2, SSD1306_BLACK);
        display.drawLine(screenX, screenY, screenX + 1, screenY - 2, SSD1306_BLACK);
      }
      break;
    case DOWN:
      if (pacman.animFrame < 2) {
        display.drawLine(screenX, screenY, screenX - 1, screenY + 2, SSD1306_BLACK);
        display.drawLine(screenX, screenY, screenX + 1, screenY + 2, SSD1306_BLACK);
      }
      break;
  }
}

void PacManGame::drawGhosts() {
  for (int i = 0; i < 3; i++) {
    if (!ghosts[i].active) continue;
    
    int screenX = MAZE_OFFSET_X + ghosts[i].x * CELL_SIZE + CELL_SIZE/2;
    int screenY = MAZE_OFFSET_Y + ghosts[i].y * CELL_SIZE + CELL_SIZE/2;
    
    // Si en mode frightened et clignotement actif, ne pas dessiner
    if (ghosts[i].frightened && !blinkState) {
      continue;  // Clignotement: sauter le dessin
    }
    
    // Dessiner fantôme agrandi (rectangle avec forme de fantôme)
    // Corps principal du fantôme
    display.fillRect(screenX - 2, screenY - 2, 5, 4, SSD1306_WHITE);
    
    // Bas ondulé du fantôme (petits creux)
    display.drawPixel(screenX - 1, screenY + 2, SSD1306_BLACK);
    display.drawPixel(screenX + 1, screenY + 2, SSD1306_BLACK);
    
    // Yeux du fantôme (plus visibles)
    display.drawPixel(screenX - 1, screenY - 1, SSD1306_BLACK);
    display.drawPixel(screenX + 1, screenY - 1, SSD1306_BLACK);
  }
}

void PacManGame::drawUI() {
  // Score
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Score:");
  display.print(score);
  
  // Vies
  display.setCursor(0, 57);
  display.print("Lives:");
  display.print(lives);
  
  // Pellets restants
  display.setCursor(70, 57);
  display.print("Left:");
  display.print(pelletsRemaining);
}

bool PacManGame::shouldReturnToMenu() {
  return requestReturnToMenu;
}