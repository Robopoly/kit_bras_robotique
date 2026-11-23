#include "space_invaders_game.h"

SpaceInvadersGame::SpaceInvadersGame() :
  invaderDirection(1), lastInvaderMove(0), lastInvaderShoot(0),
  score(0), lives(3), waveNumber(1), invaderShootDelay(1000), 
  gameOver(false), calibrated(false),
  lastMenuButtonTime(0), lastMenuBtn(HIGH), requestReturnToMenu(false) {}

void SpaceInvadersGame::init() {
  Serial.println("Space Invaders: Initialisation commencée");
  initGame();
  calibrated = false;
  requestReturnToMenu = false;
  lastMenuButtonTime = millis(); // Pas de délai initial pour éviter les problèmes
  lastMenuBtn = HIGH; // Réinitialiser l'état du bouton
  
  // Debug: vérifier l'état initial du bouton
  bool initialBtn = digitalRead(JOY1_BTN);
  Serial.print("Space Invaders: État initial bouton JOY1_BTN (GPIO5): ");
  Serial.println(initialBtn == LOW ? "PRESSED" : "RELEASED");
  Serial.println("Space Invaders: Initialisation terminée");
}

void SpaceInvadersGame::initGame() {
  // Initialisation du joueur
  player.x = SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2;
  player.y = SCREEN_HEIGHT - PLAYER_HEIGHT - 2;
  player.alive = true;
  player.autoFire = false;
  player.tripleShot = false;
  player.autoFireLastShot = 0;
  player.powerUpEndTime = 0;
  
  // Initialisation des balles
  for (int i = 0; i < MAX_BULLETS + MAX_INVADER_BULLETS; i++) {
    bullets[i].active = false;
  }
  
  // Initialisation des power-ups
  for (int i = 0; i < MAX_POWERUPS; i++) {
    powerUps[i].active = false;
  }
  
  score = 0;
  lives = 3;
  waveNumber = 1;
  invaderShootDelay = 1000; // Délai initial
  gameOver = false;
  invaderDirection = 1;
  
  initWave();
}

void SpaceInvadersGame::initWave() {
  // Initialisation des envahisseurs pour la nouvelle vague
  for (int row = 0; row < INVADERS_ROWS; row++) {
    for (int col = 0; col < INVADERS_COLS; col++) {
      invaders[row][col].x = 10 + col * 12;
      invaders[row][col].y = 8 + row * 8;
      invaders[row][col].alive = true;
    }
  }
  
  // Augmentation de la difficulté : réduction du délai de tir de 15% par vague
  invaderShootDelay = max(200, (int)(1000 * pow(0.85, waveNumber - 1)));
  
  Serial.print("Wave ");
  Serial.print(waveNumber);
  Serial.print(" - Délai tir ennemis: ");
  Serial.println(invaderShootDelay);
}

void SpaceInvadersGame::update() {
  // Vérifier demande de retour au menu avec debouncing
  bool menuBtn = digitalRead(JOY1_BTN); // Bouton joystick 1 pour retour menu
  unsigned long currentTime = millis();
  
  // Debug: afficher l'état du bouton si différent
  static bool debugDisplayed = false;
  if (menuBtn != lastMenuBtn && !debugDisplayed) {
    Serial.print("Space Invaders - Bouton menu: ");
    Serial.print(menuBtn == LOW ? "PRESSED" : "RELEASED");
    Serial.print(" Time: ");
    Serial.print(currentTime);
    Serial.print(" LastTime: ");
    Serial.println(lastMenuButtonTime);
    debugDisplayed = true;
  }
  if (menuBtn == lastMenuBtn) debugDisplayed = false;
  
  // Protection: ne permettre le retour au menu qu'après un délai initial
  if (menuBtn == LOW && lastMenuBtn == HIGH && (currentTime - lastMenuButtonTime > MENU_BUTTON_DEBOUNCE)) {
    Serial.println("Space Invaders - Retour au menu demandé!");
    requestReturnToMenu = true;
    lastMenuButtonTime = currentTime;
    return;
  }
  lastMenuBtn = menuBtn;
  
  if (!gameOver) {
    updatePlayer();
    updateBullets();
    updateInvaders();
    updatePowerUps();
    updatePlayerPowerUps();
    checkCollisions();
    
    // Vérifier si la vague est terminée
    if (checkWaveComplete()) {
      waveNumber++;
      initWave();
      Serial.print("Nouvelle vague: ");
      Serial.println(waveNumber);
    }
  }
}

void SpaceInvadersGame::updatePlayer() {
  static bool lastFireBtn = HIGH;
  unsigned long currentTime = millis(); // Ajout de cette ligne
  
  // Lecture joystick 1 axe horizontal pour déplacement droite-gauche
  int joystickH = analogRead(JOY1_H_PIN); // GPIO2 - axe horizontal du joystick 1
  bool fireBtn = digitalRead(JOYSTICK_BTN_PIN);
  
  // Calibration automatique du centre (première fois)
  if (!calibrated) {
    joystickCenter = joystickH;
    calibrated = true;
  }
  
  // Déplacement horizontal avec joystick 1 axe horizontal (INVERSÉ)
  if (joystickH < (joystickCenter - JOYSTICK_DEADZONE) && player.x < SCREEN_WIDTH - PLAYER_WIDTH) {
    // Mouvement vers la DROITE (joystick vers la gauche)
    int speed = map(joystickH, 0, joystickCenter - JOYSTICK_DEADZONE, 3, 1);
    player.x += speed;
    if (player.x > SCREEN_WIDTH - PLAYER_WIDTH) player.x = SCREEN_WIDTH - PLAYER_WIDTH;
  }
  else if (joystickH > (joystickCenter + JOYSTICK_DEADZONE) && player.x > 0) {
    // Mouvement vers la GAUCHE (joystick vers la droite)
    int speed = map(joystickH, joystickCenter + JOYSTICK_DEADZONE, 4095, 1, 3);
    player.x -= speed;
    if (player.x < 0) player.x = 0;
  }
  
  // Tir avec le bouton du 2e joystick
  if (fireBtn == LOW && lastFireBtn == HIGH) {
    if (player.tripleShot) {
      fireTripleShot(player.x + PLAYER_WIDTH / 2, player.y);
    } else {
      fireBullet(player.x + PLAYER_WIDTH / 2, player.y, true);
    }
  }
  lastFireBtn = fireBtn;
  
  // Tir automatique
  if (player.autoFire && currentTime - player.autoFireLastShot > 150) {
    if (player.tripleShot) {
      fireTripleShot(player.x + PLAYER_WIDTH / 2, player.y);
    } else {
      fireBullet(player.x + PLAYER_WIDTH / 2, player.y, true);
    }
    player.autoFireLastShot = currentTime;
  }
}

void SpaceInvadersGame::updateBullets() {
  for (int i = 0; i < MAX_BULLETS + MAX_INVADER_BULLETS; i++) {
    if (bullets[i].active) {
      if (bullets[i].fromPlayer) {
        bullets[i].y -= 3;
        if (bullets[i].y < 0) {
          bullets[i].active = false;
        }
      } else {
        bullets[i].y += 2;
        if (bullets[i].y > SCREEN_HEIGHT) {
          bullets[i].active = false;
        }
      }
    }
  }
}

void SpaceInvadersGame::updateInvaders() {
  unsigned long currentTime = millis();
  
  // Déplacement des envahisseurs
  if (currentTime - lastInvaderMove > 500) {
    bool changeDirection = false;
    
    for (int row = 0; row < INVADERS_ROWS; row++) {
      for (int col = 0; col < INVADERS_COLS; col++) {
        if (invaders[row][col].alive) {
          if ((invaderDirection > 0 && invaders[row][col].x >= SCREEN_WIDTH - INVADER_WIDTH) ||
              (invaderDirection < 0 && invaders[row][col].x <= 0)) {
            changeDirection = true;
            break;
          }
        }
      }
      if (changeDirection) break;
    }
    
    if (changeDirection) {
      invaderDirection *= -1;
      for (int row = 0; row < INVADERS_ROWS; row++) {
        for (int col = 0; col < INVADERS_COLS; col++) {
          if (invaders[row][col].alive) {
            invaders[row][col].y += 4;
            if (invaders[row][col].y + INVADER_HEIGHT >= player.y) {
              gameOver = true;
            }
          }
        }
      }
    } else {
      for (int row = 0; row < INVADERS_ROWS; row++) {
        for (int col = 0; col < INVADERS_COLS; col++) {
          if (invaders[row][col].alive) {
            invaders[row][col].x += invaderDirection;
          }
        }
      }
    }
    lastInvaderMove = currentTime;
  }
  
  // Tir des envahisseurs (avec difficulté progressive)
  if (currentTime - lastInvaderShoot > invaderShootDelay) {
    invaderShoot();
    lastInvaderShoot = currentTime;
  }
}

void SpaceInvadersGame::checkCollisions() {
  for (int i = 0; i < MAX_BULLETS + MAX_INVADER_BULLETS; i++) {
    if (!bullets[i].active) continue;
    
    if (bullets[i].fromPlayer) {
      for (int row = 0; row < INVADERS_ROWS; row++) {
        for (int col = 0; col < INVADERS_COLS; col++) {
          if (invaders[row][col].alive &&
              bullets[i].x >= invaders[row][col].x &&
              bullets[i].x <= invaders[row][col].x + INVADER_WIDTH &&
              bullets[i].y >= invaders[row][col].y &&
              bullets[i].y <= invaders[row][col].y + INVADER_HEIGHT) {
            invaders[row][col].alive = false;
            bullets[i].active = false;
            
            // Calcul du score selon la ligne
            switch (row) {
              case 0: score += 40; break;
              case 1: score += 30; break;
              case 2: score += 20; break;
              case 3: score += 10; break;
              default: score += 10; break;
            }
            
            // Génération de power-up (20% de chance à partir de la vague 3)
            if (waveNumber >= 3 && random(0, 100) < 20) {
              spawnPowerUp(invaders[row][col].x + INVADER_WIDTH/2, invaders[row][col].y);
            }
            
            break;
          }
        }
      }
    } else {
      if (bullets[i].x >= player.x &&
          bullets[i].x <= player.x + PLAYER_WIDTH &&
          bullets[i].y >= player.y &&
          bullets[i].y <= player.y + PLAYER_HEIGHT) {
        bullets[i].active = false;
        lives--;
        if (lives <= 0) {
          gameOver = true;
        }
      }
    }
  }
  
  // Collision joueur avec power-ups
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (powerUps[i].active &&
        powerUps[i].x >= player.x - 3 &&
        powerUps[i].x <= player.x + PLAYER_WIDTH + 3 &&
        powerUps[i].y >= player.y - 3 &&
        powerUps[i].y <= player.y + PLAYER_HEIGHT + 3) {
      collectPowerUp(i);
    }
  }
}

void SpaceInvadersGame::draw() {
  display.clearDisplay();
  
  if (gameOver) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("GAME OVER");
    display.setTextSize(1);
    display.setCursor(20, 35);
    display.print("Score: ");
    display.println(score);
    display.setCursor(20, 45);
    display.print("Wave: ");
    display.println(waveNumber);
    display.setCursor(15, 55);
    display.println("BTN1: Menu");
  } else {
    drawPlayer();
    drawBullets();
    drawInvaders();
    drawPowerUps();
    drawUI();
    
    // Instructions
    display.setTextSize(1);
    display.setCursor(5, SCREEN_HEIGHT - 8);
    display.println("BTN1: Menu");
  }
  
  display.display();
}

void SpaceInvadersGame::drawPlayer() {
  if (player.alive) {
    // Vaisseau triangulaire
    display.fillTriangle(
      player.x + PLAYER_WIDTH/2, player.y,
      player.x, player.y + PLAYER_HEIGHT,
      player.x + PLAYER_WIDTH, player.y + PLAYER_HEIGHT,
      SSD1306_WHITE
    );
    
    // Cockpit
    display.fillCircle(player.x + PLAYER_WIDTH/2, player.y + PLAYER_HEIGHT/2, 1, SSD1306_WHITE);
    
    // Réacteurs
    display.drawPixel(player.x + 1, player.y + PLAYER_HEIGHT, SSD1306_WHITE);
    display.drawPixel(player.x + PLAYER_WIDTH - 1, player.y + PLAYER_HEIGHT, SSD1306_WHITE);
  }
}

void SpaceInvadersGame::drawBullets() {
  for (int i = 0; i < MAX_BULLETS + MAX_INVADER_BULLETS; i++) {
    if (bullets[i].active) {
      if (i < MAX_BULLETS) {
        // Lasers joueur
        display.drawLine(bullets[i].x, bullets[i].y, 
                        bullets[i].x, bullets[i].y + BULLET_HEIGHT, SSD1306_WHITE);
        display.drawLine(bullets[i].x + 1, bullets[i].y, 
                        bullets[i].x + 1, bullets[i].y + BULLET_HEIGHT, SSD1306_WHITE);
      } else {
        // Balles ennemies
        display.fillCircle(bullets[i].x, bullets[i].y + BULLET_HEIGHT/2, 1, SSD1306_WHITE);
      }
    }
  }
}

void SpaceInvadersGame::drawInvaders() {
  for (int row = 0; row < INVADERS_ROWS; row++) {
    for (int col = 0; col < INVADERS_COLS; col++) {
      if (invaders[row][col].alive) {
        int x = invaders[row][col].x;
        int y = invaders[row][col].y;
        
        switch (row) {
          case 0: // Diamant
            display.fillTriangle(x + INVADER_WIDTH/2, y, 
                               x, y + INVADER_HEIGHT/2, 
                               x + INVADER_WIDTH/2, y + INVADER_HEIGHT, SSD1306_WHITE);
            display.fillTriangle(x + INVADER_WIDTH/2, y, 
                               x + INVADER_WIDTH, y + INVADER_HEIGHT/2, 
                               x + INVADER_WIDTH/2, y + INVADER_HEIGHT, SSD1306_WHITE);
            break;
            
          case 1: // Hexagone
            display.drawLine(x + 1, y, x + INVADER_WIDTH - 1, y, SSD1306_WHITE);
            display.drawLine(x, y + 1, x, y + INVADER_HEIGHT - 1, SSD1306_WHITE);
            display.drawLine(x + INVADER_WIDTH, y + 1, x + INVADER_WIDTH, y + INVADER_HEIGHT - 1, SSD1306_WHITE);
            display.drawLine(x + 1, y + INVADER_HEIGHT, x + INVADER_WIDTH - 1, y + INVADER_HEIGHT, SSD1306_WHITE);
            display.fillRect(x + 1, y + 1, INVADER_WIDTH - 1, INVADER_HEIGHT - 1, SSD1306_WHITE);
            break;
            
          case 2: // Cercle avec antennes
            display.fillCircle(x + INVADER_WIDTH/2, y + INVADER_HEIGHT/2, INVADER_WIDTH/2, SSD1306_WHITE);
            display.drawPixel(x + 1, y, SSD1306_WHITE);
            display.drawPixel(x + INVADER_WIDTH - 1, y, SSD1306_WHITE);
            break;
            
          case 3: // Rectangle avec yeux
          default:
            display.fillRect(x, y, INVADER_WIDTH, INVADER_HEIGHT, SSD1306_WHITE);
            display.drawPixel(x + 1, y + 1, SSD1306_BLACK);
            display.drawPixel(x + INVADER_WIDTH - 2, y + 1, SSD1306_BLACK);
            break;
        }
      }
    }
  }
}

void SpaceInvadersGame::drawUI() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Score:");
  display.print(score);
  display.setCursor(60, 0);
  display.print("Lives:");
  display.print(lives);
  display.setCursor(100, 0);
  display.print("W:");
  display.print(waveNumber);
  
  // Indicateurs de power-ups actifs
  if (player.autoFire || player.tripleShot) {
    display.setCursor(0, SCREEN_HEIGHT - 16);
    if (player.autoFire) display.print("AUTO ");
    if (player.tripleShot) display.print("x3 ");
    
    // Temps restant
    unsigned long timeLeft = (player.powerUpEndTime - millis()) / 1000;
    if (timeLeft < 60) {
      display.print(timeLeft);
      display.print("s");
    }
  }
}

void SpaceInvadersGame::fireBullet(int x, int y, bool fromPlayer) {
  int startIndex = fromPlayer ? 0 : MAX_BULLETS;
  int endIndex = fromPlayer ? MAX_BULLETS : MAX_BULLETS + MAX_INVADER_BULLETS;
  
  for (int i = startIndex; i < endIndex; i++) {
    if (!bullets[i].active) {
      bullets[i].x = x;
      bullets[i].y = y;
      bullets[i].active = true;
      bullets[i].fromPlayer = fromPlayer;
      break;
    }
  }
}

void SpaceInvadersGame::invaderShoot() {
  for (int col = 0; col < INVADERS_COLS; col++) {
    for (int row = INVADERS_ROWS - 1; row >= 0; row--) {
      if (invaders[row][col].alive && random(0, 20) == 0) {
        fireBullet(invaders[row][col].x + INVADER_WIDTH / 2, 
                  invaders[row][col].y + INVADER_HEIGHT, false);
        return;
      }
    }
  }
}

bool SpaceInvadersGame::checkWaveComplete() {
  for (int row = 0; row < INVADERS_ROWS; row++) {
    for (int col = 0; col < INVADERS_COLS; col++) {
      if (invaders[row][col].alive) {
        return false;
      }
    }
  }
  return true;
}

// Nouvelles fonctions pour les power-ups et fonctionnalités étendues

void SpaceInvadersGame::fireTripleShot(int x, int y) {
  // Tir central
  fireBullet(x, y, true);
  // Tirs diagonaux
  fireBullet(x - 3, y, true);
  fireBullet(x + 3, y, true);
}

void SpaceInvadersGame::updatePowerUps() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (powerUps[i].active) {
      powerUps[i].y += 1; // Vitesse de chute des power-ups
      
      // Supprimer si sort de l'écran
      if (powerUps[i].y > SCREEN_HEIGHT) {
        powerUps[i].active = false;
      }
    }
  }
}

void SpaceInvadersGame::updatePlayerPowerUps() {
  unsigned long currentTime = millis();
  
  // Vérifier si les power-ups ont expiré
  if ((player.autoFire || player.tripleShot) && currentTime > player.powerUpEndTime) {
    player.autoFire = false;
    player.tripleShot = false;
    Serial.println("Power-up expiré");
  }
}

void SpaceInvadersGame::spawnPowerUp(int x, int y) {
  // Trouver un slot libre
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (!powerUps[i].active) {
      powerUps[i].x = x;
      powerUps[i].y = y;
      powerUps[i].active = true;
      powerUps[i].spawnTime = millis();
      
      // Choisir un type de power-up aléatoire
      powerUps[i].type = (PowerUpType)random(0, 3);
      
      Serial.print("Power-up spawné: ");
      Serial.println(powerUps[i].type);
      break;
    }
  }
}

void SpaceInvadersGame::collectPowerUp(int index) {
  if (!powerUps[index].active) return;
  
  PowerUpType type = powerUps[index].type;
  powerUps[index].active = false;
  
  unsigned long currentTime = millis();
  
  switch (type) {
    case TRIPLE_SHOT:
      player.tripleShot = true;
      player.autoFire = false; // Reset auto fire si actif
      player.powerUpEndTime = currentTime + 10000; // 10 secondes
      player.activePowerUp = TRIPLE_SHOT;
      Serial.println("Triple shot activé!");
      break;
      
    case AUTO_FIRE:
      player.autoFire = true;
      player.tripleShot = false; // Reset triple shot si actif
      player.powerUpEndTime = currentTime + 10000; // 10 secondes
      player.activePowerUp = AUTO_FIRE;
      player.autoFireLastShot = currentTime;
      Serial.println("Auto fire activé!");
      break;
      
    case EXTRA_LIFE:
      if (lives < MAX_LIVES) {
        lives++;
        Serial.println("Vie supplémentaire!");
      } else {
        score += 100; // Bonus si déjà au max
        Serial.println("Bonus score: +100");
      }
      break;
  }
}

void SpaceInvadersGame::drawPowerUps() {
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (powerUps[i].active) {
      int x = powerUps[i].x;
      int y = powerUps[i].y;
      
      switch (powerUps[i].type) {
        case TRIPLE_SHOT:
          // Dessiner 3 petites lignes pour tir multiple
          display.drawLine(x-2, y, x-2, y+2, SSD1306_WHITE);
          display.drawLine(x, y, x, y+2, SSD1306_WHITE);
          display.drawLine(x+2, y, x+2, y+2, SSD1306_WHITE);
          break;
          
        case AUTO_FIRE:
          // Dessiner un carré clignotant pour auto fire
          if ((millis() / 200) % 2) {
            display.fillRect(x-1, y-1, 3, 3, SSD1306_WHITE);
          }
          break;
          
        case EXTRA_LIFE:
          // Dessiner un petit cœur pour vie supplémentaire
          display.drawPixel(x-1, y, SSD1306_WHITE);
          display.drawPixel(x+1, y, SSD1306_WHITE);
          display.drawPixel(x, y+1, SSD1306_WHITE);
          display.drawPixel(x-1, y+1, SSD1306_WHITE);
          display.drawPixel(x+1, y+1, SSD1306_WHITE);
          display.drawPixel(x, y+2, SSD1306_WHITE);
          break;
      }
    }
  }
}

bool SpaceInvadersGame::shouldReturnToMenu() {
  return requestReturnToMenu;
}