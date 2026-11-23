#include "pong_game.h"

// Instance statique pour les callbacks ESP-NOW
PongGame* PongGame::instance = nullptr;

PongGame::PongGame() : 
  paddle1_y((SCREEN_HEIGHT - PADDLE_HEIGHT) / 2),
  paddle2_y((SCREEN_HEIGHT - PADDLE_HEIGHT) / 2),
  ball_xf(SCREEN_WIDTH / 2), ball_yf(SCREEN_HEIGHT / 2),
  ball_dx(1), ball_dy(0), ball_speed(3.0), base_ball_speed(3.0),
  score1(0), score2(0), maxPoints(5),
  gameState(0), menuIndex(0), lastJoy2Btn(HIGH),
  editingMaxPoints(false), blinkTimer(0), blinkState(true),
  lastMenuButtonTime(0), requestReturnToMenu(false), lastJoy1Btn(HIGH),
  gameStartTime(0), lastSpeedIncreaseTime(0),
  // Multijoueur
  pongMode(PONG_MODE_LOCAL), espNowInitialized(false), peerConnected(false),
  lastPeerMessage(0), lastStateSent(0), lastDiscoverySent(0),
  newGameStateReceived(false), newPaddleMsgReceived(false),
  motorsInitialized(false), motorLeftOffTime(0), motorRightOffTime(0) {
  
  instance = this;
  memset(peerMAC, 0, 6);
}

void PongGame::init() {
  paddle1_y = (SCREEN_HEIGHT - PADDLE_HEIGHT) / 2;
  paddle2_y = (SCREEN_HEIGHT - PADDLE_HEIGHT) / 2;
  score1 = 0;
  score2 = 0;
  gameState = 0; // Menu principal
  menuIndex = 0;
  editingMaxPoints = false;
  requestReturnToMenu = false;
  pongMode = PONG_MODE_LOCAL;
  peerConnected = false;
  newGameStateReceived = false;
  newPaddleMsgReceived = false;
  
  // CRITIQUE: réinitialiser les états des boutons pour éviter les "fantômes"
  // venant du menu principal (ex: GPIO6 encore enfoncé à l'entrée dans Pong)
  lastJoy2Btn = digitalRead(JOY2_BTN); // lire l'état réel actuel, pas supposer HIGH
  lastJoy1Btn = digitalRead(JOY1_BTN); // idem pour GPIO5
  lastMenuButtonTime = millis();        // debounce part de maintenant, pas de t=0

  // Désactiver ESP-NOW si actif
  if (espNowInitialized) {
    deinitESPNow();
  }

  initHaptics();
  digitalWrite(MOTOR_LEFT_PIN, LOW);
  digitalWrite(MOTOR_RIGHT_PIN, LOW);
  motorLeftOffTime = 0;
  motorRightOffTime = 0;
  
  resetBall();
}

void PongGame::initHaptics() {
  if (motorsInitialized) return;
  pinMode(MOTOR_LEFT_PIN, OUTPUT);
  pinMode(MOTOR_RIGHT_PIN, OUTPUT);
  digitalWrite(MOTOR_LEFT_PIN, LOW);
  digitalWrite(MOTOR_RIGHT_PIN, LOW);
  motorsInitialized = true;
}

void PongGame::updateHaptics() {
  if (!motorsInitialized) return;
  unsigned long now = millis();

  if (motorLeftOffTime > 0 && now >= motorLeftOffTime) {
    digitalWrite(MOTOR_LEFT_PIN, LOW);
    motorLeftOffTime = 0;
  }

  if (motorRightOffTime > 0 && now >= motorRightOffTime) {
    digitalWrite(MOTOR_RIGHT_PIN, LOW);
    motorRightOffTime = 0;
  }
}

void PongGame::triggerLeftHaptic() {
  if (!motorsInitialized) return;
  digitalWrite(MOTOR_LEFT_PIN, HIGH);
  motorLeftOffTime = millis() + HAPTIC_PULSE_MS;
}

void PongGame::triggerRightHaptic() {
  if (!motorsInitialized) return;
  digitalWrite(MOTOR_RIGHT_PIN, HIGH);
  motorRightOffTime = millis() + HAPTIC_PULSE_MS;
}

void PongGame::resetBall() {
  ball_xf = SCREEN_WIDTH / 2;
  ball_yf = SCREEN_HEIGHT / 2;
  ball_dx = (random(2) == 0) ? -1 : 1;
  ball_dy = 0;
  ball_x = (int)ball_xf;
  ball_y = (int)ball_yf;
}

// ============================================
// ESP-NOW - INITIALISATION ET CALLBACKS
// ============================================

void PongGame::initESPNow() {
  if (espNowInitialized) return;
  
  Serial.println("Pong: Initialisation ESP-NOW...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("Pong: MAC locale = ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Pong: ERREUR init ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(onDataSentStatic);
  esp_now_register_recv_cb(onDataReceivedStatic);
  
  // Ajouter le peer broadcast pour découverte
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Pong: ERREUR ajout peer broadcast");
  }
  
  espNowInitialized = true;
  Serial.println("Pong: ESP-NOW initialisé!");
}

void PongGame::deinitESPNow() {
  if (!espNowInitialized) return;
  
  Serial.println("Pong: Fermeture ESP-NOW...");
  
  removePeer();
  esp_now_del_peer(BROADCAST_MAC);
  esp_now_deinit();
  espNowInitialized = false;
  peerConnected = false;
}

void PongGame::addPeer(const uint8_t* mac) {
  // Vérifier si c'est le broadcast
  bool isBroadcast = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) { isBroadcast = false; break; }
  }
  if (isBroadcast) return;
  
  // Supprimer l'ancien peer si existant
  removePeer();
  
  // Copier la MAC
  memcpy(peerMAC, mac, 6);
  
  // Ajouter le nouveau peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, peerMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.print("Pong: Peer ajouté: ");
    for (int i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      Serial.printf("%02X", peerMAC[i]);
    }
    Serial.println();
    peerConnected = true;
    lastPeerMessage = millis();
  }
}

void PongGame::removePeer() {
  if (peerMAC[0] != 0 || peerMAC[1] != 0) {
    esp_now_del_peer(peerMAC);
    memset(peerMAC, 0, 6);
  }
  peerConnected = false;
}

// Callbacks statiques
void PongGame::onDataSentStatic(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (instance) {
    instance->onDataSent(mac_addr, status);
  }
}

void PongGame::onDataReceivedStatic(const uint8_t *mac, const uint8_t *data, int len) {
  if (instance) {
    instance->onDataReceived(mac, data, len);
  }
}

void PongGame::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Debug optionnel
  // Serial.print("Pong Send: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void PongGame::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < 1) return;
  
  uint8_t msgType = data[0];
  
  switch (msgType) {
    case PONG_MSG_DISCOVERY:
    case PONG_MSG_DISCOVERY_ACK: {
      if (len >= sizeof(PongDiscoveryMsg)) {
        PongDiscoveryMsg* msg = (PongDiscoveryMsg*)data;
        
        Serial.print("Pong: Reçu DISCOVERY de ");
        for (int i = 0; i < 6; i++) {
          if (i > 0) Serial.print(":");
          Serial.printf("%02X", mac[i]);
        }
        Serial.print(" isHost=");
        Serial.println(msg->isHost);
        
        // Si on est en attente de connexion
        if (gameState == 4) { // WAITING_CONNECTION
          // Host reçoit une réponse de Client
          if (pongMode == PONG_MODE_HOST && msg->isHost == 0) {
            addPeer(mac);
            // Envoyer un ACK
            PongDiscoveryMsg ack;
            ack.msgType = PONG_MSG_DISCOVERY_ACK;
            ack.isHost = 1;
            ack.timestamp = millis();
            esp_now_send(peerMAC, (uint8_t*)&ack, sizeof(ack));
          }
          // Client reçoit une demande de Host
          else if (pongMode == PONG_MODE_CLIENT && msg->isHost == 1) {
            addPeer(mac);
            // Envoyer une réponse
            PongDiscoveryMsg response;
            response.msgType = PONG_MSG_DISCOVERY_ACK;
            response.isHost = 0;
            response.timestamp = millis();
            esp_now_send(peerMAC, (uint8_t*)&response, sizeof(response));
          }
        }
      }
      break;
    }
    
    case PONG_MSG_GAME_STATE: {
      if (len >= sizeof(PongGameState) && pongMode == PONG_MODE_CLIENT) {
        memcpy(&receivedGameState, data, sizeof(PongGameState));
        newGameStateReceived = true;
        lastPeerMessage = millis();
      }
      break;
    }
    
    case PONG_MSG_PADDLE_POS: {
      if (len >= sizeof(PongPaddleMsg) && pongMode == PONG_MODE_HOST) {
        memcpy(&receivedPaddleMsg, data, sizeof(PongPaddleMsg));
        newPaddleMsgReceived = true;
        lastPeerMessage = millis();
      }
      break;
    }
    
    case PONG_MSG_GAME_START:
    case PONG_MSG_GAME_END:
      lastPeerMessage = millis();
      break;
  }
}

// ============================================
// ENVOI DE DONNÉES
// ============================================

void PongGame::sendDiscovery(bool isHost) {
  PongDiscoveryMsg msg;
  msg.msgType = PONG_MSG_DISCOVERY;
  msg.isHost = isHost ? 1 : 0;
  msg.timestamp = millis();
  
  esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
}

void PongGame::sendGameState() {
  if (!peerConnected) return;
  
  PongGameState state;
  state.msgType = PONG_MSG_GAME_STATE;
  state.ballX = (int16_t)(ball_xf * 100);
  state.ballY = (int16_t)(ball_yf * 100);
  state.ballDX = (int16_t)(ball_dx * 100);
  state.ballDY = (int16_t)(ball_dy * 100);
  state.paddle1Y = (uint8_t)paddle1_y;
  state.paddle2Y = (uint8_t)paddle2_y;
  state.score1 = (uint8_t)score1;
  state.score2 = (uint8_t)score2;
  state.gameState = (uint8_t)gameState;
  state.maxPoints = (uint8_t)maxPoints;
  state.timestamp = millis();
  
  esp_now_send(peerMAC, (uint8_t*)&state, sizeof(state));
}

void PongGame::sendPaddlePosition() {
  if (!peerConnected) return;
  
  PongPaddleMsg msg;
  msg.msgType = PONG_MSG_PADDLE_POS;
  msg.paddleY = (uint8_t)paddle2_y; // Le client contrôle paddle2
  msg.buttonPressed = digitalRead(JOY2_BTN) == LOW ? 1 : 0;
  msg.timestamp = millis();
  
  esp_now_send(peerMAC, (uint8_t*)&msg, sizeof(msg));
}

// ============================================
// UPDATE
// ============================================

void PongGame::update() {
  bool joy2Btn = digitalRead(JOY2_BTN);
  bool joy1Btn = digitalRead(JOY1_BTN);
  int joy1_v = analogRead(JOY1_V);
  int joy2_v = analogRead(JOY2_V);
  unsigned long currentTime = millis();
  updateHaptics();
  
  // Clignotement pour les menus
  if (currentTime - blinkTimer > 400) {
    blinkState = !blinkState;
    blinkTimer = currentTime;
  }
  
  // État 0: MENU PRINCIPAL PONG
  if (gameState == 0) {
    // Navigation menu (3 options: Local, Host, Client) - sens inversé
    if (!editingMaxPoints) {
      if (joy1_v < 1000 && menuIndex < 3) { menuIndex++; delay(200); }
      if (joy1_v > 3000 && menuIndex > 0) { menuIndex--; delay(200); }
    }
    
    // Modification Max Points - sens inversé
    if (editingMaxPoints) {
      if (joy1_v < 1000 && maxPoints < 15) { maxPoints++; delay(150); }
      if (joy1_v > 3000 && maxPoints > 1) { maxPoints--; delay(150); }
      
      if (joy2Btn == LOW && lastJoy2Btn == HIGH) {
        editingMaxPoints = false;
      }
    }
    
    // Actions menu
    if (joy2Btn == LOW && lastJoy2Btn == HIGH && !editingMaxPoints) {
      switch (menuIndex) {
        case 0: // Jouer LOCAL
          pongMode = PONG_MODE_LOCAL;
          gameState = 1;
          score1 = 0; score2 = 0;
          ball_speed = base_ball_speed;
          gameStartTime = millis();
          lastSpeedIncreaseTime = gameStartTime;
          resetBall();
          Serial.println("Pong: Mode LOCAL");
          break;
          
        case 1: // Créer partie (HOST)
          pongMode = PONG_MODE_HOST;
          gameState = 4; // Attente connexion
          initESPNow();
          lastDiscoverySent = 0;
          peerConnected = false;
          Serial.println("Pong: Mode HOST - En attente...");
          break;
          
        case 2: // Rejoindre (CLIENT)
          pongMode = PONG_MODE_CLIENT;
          gameState = 4; // Attente connexion
          initESPNow();
          lastDiscoverySent = 0;
          peerConnected = false;
          Serial.println("Pong: Mode CLIENT - Recherche...");
          break;
          
        case 3: // Max Points
          editingMaxPoints = true;
          blinkState = true;
          blinkTimer = millis();
          break;
      }
    }
    
    // Retour menu principal - TRANSITION HIGH→LOW requise (comme tous les autres jeux)
    if (joy1Btn == LOW && lastJoy1Btn == HIGH && currentTime - lastMenuButtonTime > MENU_BUTTON_DEBOUNCE) {
      requestReturnToMenu = true;
      lastMenuButtonTime = currentTime;
    }
  }
  
  // État 4: ATTENTE CONNEXION
  else if (gameState == 4) {
    updateWaitingConnection();
    
    // Annuler avec bouton - TRANSITION HIGH→LOW requise
    if (joy1Btn == LOW && lastJoy1Btn == HIGH && currentTime - lastMenuButtonTime > MENU_BUTTON_DEBOUNCE) {
      deinitESPNow();
      gameState = 0;
      lastMenuButtonTime = currentTime;
      Serial.println("Pong: Connexion annulée");
    }
  }
  
  // État 1: JEU EN COURS
  else if (gameState == 1) {
    // Gestion selon le mode
    if (pongMode == PONG_MODE_LOCAL) {
      // Mode local: les deux raquettes sont contrôlées localement
      if (currentTime - lastSpeedIncreaseTime >= 15000) {
        ball_speed *= 1.15;
        lastSpeedIncreaseTime = currentTime;
      }
      updatePaddles();
      updateBall();
      checkCollisions();
    }
    else if (pongMode == PONG_MODE_HOST) {
      updateAsHost();
    }
    else if (pongMode == PONG_MODE_CLIENT) {
      updateAsClient();
    }
    
    // Vérifier victoire
    if (score1 >= maxPoints || score2 >= maxPoints) {
      gameState = 2; // Game Over
    }
    
    // Retour au menu (BTN2)
    if (joy2Btn == LOW && lastJoy2Btn == HIGH) {
      if (currentTime - lastMenuButtonTime > MENU_BUTTON_DEBOUNCE) {
        if (pongMode != PONG_MODE_LOCAL) {
          deinitESPNow();
        }
        gameState = 0;
        lastMenuButtonTime = currentTime;
      }
    }
  }
  
  // État 2: GAME OVER
  else if (gameState == 2) {
    if (joy2Btn == LOW && lastJoy2Btn == HIGH) {
      if (pongMode != PONG_MODE_LOCAL) {
        deinitESPNow();
      }
      gameState = 0; // Retour menu
    }
  }
  
  lastJoy2Btn = joy2Btn;
  lastJoy1Btn = joy1Btn;
}

void PongGame::updateWaitingConnection() {
  unsigned long currentTime = millis();
  
  // Envoyer découverte périodiquement
  if (currentTime - lastDiscoverySent > DISCOVERY_INTERVAL) {
    sendDiscovery(pongMode == PONG_MODE_HOST);
    lastDiscoverySent = currentTime;
    Serial.println("Pong: Envoi découverte...");
  }
  
  // Si connexion établie, démarrer le jeu
  if (peerConnected) {
    Serial.println("Pong: Connexion établie! Démarrage...");
    gameState = 1;
    score1 = 0; 
    score2 = 0;
    ball_speed = base_ball_speed;
    gameStartTime = millis();
    lastSpeedIncreaseTime = gameStartTime;
    lastStateSent = 0;
    resetBall();
  }
}

void PongGame::updateAsHost() {
  unsigned long currentTime = millis();
  
  // Vérifier timeout connexion
  if (currentTime - lastPeerMessage > PEER_TIMEOUT && lastPeerMessage > 0) {
    Serial.println("Pong HOST: Timeout connexion!");
    peerConnected = false;
    // Continuer le jeu mais afficher déconnexion
  }
  
  // Augmentation vitesse
  if (currentTime - lastSpeedIncreaseTime >= 15000) {
    ball_speed *= 1.15;
    lastSpeedIncreaseTime = currentTime;
  }
  
  // Le Host contrôle paddle1 (gauche)
  int joy1_v = analogRead(JOY1_V);
  if (joy1_v > 2500) {
    int deviation = joy1_v - 2500;
    float speed = (deviation / 1595.0) * 4.0;
    paddle1_y -= speed;
  }
  if (joy1_v < 1500) {
    int deviation = 1500 - joy1_v;
    float speed = (deviation / 1500.0) * 4.0;
    paddle1_y += speed;
  }
  paddle1_y = constrain(paddle1_y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  
  // Recevoir position paddle2 du Client
  if (newPaddleMsgReceived) {
    paddle2_y = receivedPaddleMsg.paddleY;
    paddle2_y = constrain(paddle2_y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
    newPaddleMsgReceived = false;
  }
  
  // Mise à jour balle et collisions
  updateBall();
  checkCollisions();
  
  // Envoyer l'état du jeu au Client
  if (currentTime - lastStateSent > STATE_SEND_INTERVAL) {
    sendGameState();
    lastStateSent = currentTime;
  }
}

void PongGame::updateAsClient() {
  unsigned long currentTime = millis();
  
  // Vérifier timeout connexion
  if (currentTime - lastPeerMessage > PEER_TIMEOUT && lastPeerMessage > 0) {
    Serial.println("Pong CLIENT: Timeout connexion!");
    peerConnected = false;
  }
  
  // Le Client contrôle paddle2 (droite) avec JOY1 (son joystick principal)
  int joy1_v = analogRead(JOY1_V);
  if (joy1_v > 2500) {
    int deviation = joy1_v - 2500;
    float speed = (deviation / 1595.0) * 4.0;
    paddle2_y -= speed;
  }
  if (joy1_v < 1500) {
    int deviation = 1500 - joy1_v;
    float speed = (deviation / 1500.0) * 4.0;
    paddle2_y += speed;
  }
  paddle2_y = constrain(paddle2_y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  
  // Envoyer position au Host
  if (currentTime - lastStateSent > STATE_SEND_INTERVAL) {
    sendPaddlePosition();
    lastStateSent = currentTime;
  }
  
  // Recevoir état du jeu depuis Host
  if (newGameStateReceived) {
    float previousBallDx = ball_dx;

    ball_xf = receivedGameState.ballX / 100.0f;
    ball_yf = receivedGameState.ballY / 100.0f;
    ball_dx = receivedGameState.ballDX / 100.0f;
    ball_dy = receivedGameState.ballDY / 100.0f;
    ball_x = (int)ball_xf;
    ball_y = (int)ball_yf;
    paddle1_y = receivedGameState.paddle1Y;
    score1 = receivedGameState.score1;
    score2 = receivedGameState.score2;
    maxPoints = receivedGameState.maxPoints;
    
    // Mise à jour gameState si nécessaire
    if (receivedGameState.gameState == 2) {
      gameState = 2; // Game Over
    }

    // Détection côté client des collisions raquettes via inversion du sens X
    // et position proche des bords pour éviter les faux positifs au reset balle.
    if (previousBallDx > 0.0f && ball_dx < 0.0f && ball_x > SCREEN_WIDTH - 20) {
      triggerRightHaptic();
    } else if (previousBallDx < 0.0f && ball_dx > 0.0f && ball_x < 20) {
      triggerLeftHaptic();
    }
    
    newGameStateReceived = false;
  }
}

void PongGame::updatePaddles() {
  // Mode local uniquement - les deux joysticks contrôlent les raquettes
  int joy1_v = analogRead(JOY1_V);
  int joy2_v = analogRead(JOY2_V);
  
  // Paddle 1 (gauche)
  if (joy1_v > 2500) {
    int deviation = joy1_v - 2500;
    float speed = (deviation / 1595.0) * 4.0;
    paddle1_y -= speed;
  }
  if (joy1_v < 1500) {
    int deviation = 1500 - joy1_v;
    float speed = (deviation / 1500.0) * 4.0;
    paddle1_y += speed;
  }
  paddle1_y = constrain(paddle1_y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  
  // Paddle 2 (droite)
  if (joy2_v > 2500) {
    int deviation = joy2_v - 2500;
    float speed = (deviation / 1595.0) * 4.0;
    paddle2_y -= speed;
  }
  if (joy2_v < 1500) {
    int deviation = 1500 - joy2_v;
    float speed = (deviation / 1500.0) * 4.0;
    paddle2_y += speed;
  }
  paddle2_y = constrain(paddle2_y, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
}

void PongGame::updateBall() {
  ball_xf += ball_dx * ball_speed;
  ball_yf += ball_dy * ball_speed;
  ball_x = (int)ball_xf;
  ball_y = (int)ball_yf;
  
  // Rebond haut/bas
  if (ball_y <= 0 || ball_y >= SCREEN_HEIGHT - BALL_SIZE) {
    ball_dy = -ball_dy;
  }
}

void PongGame::checkCollisions() {
  // Collision paddle gauche
  if (ball_x <= PADDLE_WIDTH && 
      ball_y >= paddle1_y && ball_y <= paddle1_y + PADDLE_HEIGHT) {
    triggerLeftHaptic();
    ball_dx = abs(ball_dx);
    float relativeIntersectY = (ball_y + BALL_SIZE/2) - (paddle1_y + PADDLE_HEIGHT/2);
    ball_dy = relativeIntersectY / (PADDLE_HEIGHT/2) * 2;
    
    float magnitude = sqrt(ball_dx * ball_dx + ball_dy * ball_dy);
    ball_dx /= magnitude;
    ball_dy /= magnitude;
  }
  
  // Collision paddle droite
  if (ball_x >= SCREEN_WIDTH - PADDLE_WIDTH - BALL_SIZE && 
      ball_y >= paddle2_y && ball_y <= paddle2_y + PADDLE_HEIGHT) {
    triggerRightHaptic();
    ball_dx = -abs(ball_dx);
    float relativeIntersectY = (ball_y + BALL_SIZE/2) - (paddle2_y + PADDLE_HEIGHT/2);
    ball_dy = relativeIntersectY / (PADDLE_HEIGHT/2) * 2;
    
    float magnitude = sqrt(ball_dx * ball_dx + ball_dy * ball_dy);
    ball_dx /= magnitude;
    ball_dy /= magnitude;
  }
  
  // Score
  if (ball_x < 0) { score2++; resetBall(); }
  if (ball_x > SCREEN_WIDTH) { score1++; resetBall(); }
}

// ============================================
// DRAW
// ============================================

void PongGame::draw() {
  display.clearDisplay();
  
  if (gameState == 0) {
    drawMenu();
  } else if (gameState == 1) {
    drawGame();
  } else if (gameState == 2) {
    drawGameOver((score1 >= maxPoints) ? 1 : 2);
  } else if (gameState == 4) {
    drawWaitingConnection();
  }
  
  display.display();
}

void PongGame::drawMenu() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 2);
  display.println("PONG");
  
  display.setTextSize(1);
  
  const char* menuItems[] = {"Local 2P", "Creer partie", "Rejoindre", "Max Points:"};
  int menuY[] = {20, 30, 40, 50};
  
  for (int i = 0; i < 4; i++) {
    if (menuIndex == i) {
      display.fillRect(5, menuY[i] - 1, 118, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(10, menuY[i]);
    display.print(menuItems[i]);
    
    if (i == 3) {
      // Afficher la valeur de Max Points
      if (editingMaxPoints && blinkState && menuIndex == 3) {
        display.setTextColor(SSD1306_BLACK);
      }
      display.print(maxPoints);
    }
  }
  
  // Instructions
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 56);
  display.print("BTN1:Menu BTN2:Select");
}

void PongGame::drawGame() {
  // Paddle gauche
  display.fillRect(0, paddle1_y, PADDLE_WIDTH, PADDLE_HEIGHT, SSD1306_WHITE);
  
  // Paddle droite
  display.fillRect(SCREEN_WIDTH - PADDLE_WIDTH, paddle2_y, PADDLE_WIDTH, PADDLE_HEIGHT, SSD1306_WHITE);
  
  // Balle
  display.fillRect(ball_x, ball_y, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);
  
  // Ligne centrale
  for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
    display.drawPixel(SCREEN_WIDTH/2, y, SSD1306_WHITE);
  }
  
  // Scores
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH/4 - 6, 5);
  display.print(score1);
  display.setCursor(3*SCREEN_WIDTH/4 - 6, 5);
  display.print(score2);
  
  // Indicateur de mode
  display.setTextSize(1);
  display.setCursor(5, SCREEN_HEIGHT - 10);
  
  if (pongMode == PONG_MODE_LOCAL) {
    display.print("LOCAL");
  } else if (pongMode == PONG_MODE_HOST) {
    display.print("HOST");
    if (!peerConnected && blinkState) {
      display.print(" DECONNECTE");
    }
  } else if (pongMode == PONG_MODE_CLIENT) {
    display.print("CLIENT");
    if (!peerConnected && blinkState) {
      display.print(" DECONNECTE");
    }
  }
  
  display.setCursor(90, SCREEN_HEIGHT - 10);
  display.print("BTN2:Quit");
}

void PongGame::drawWaitingConnection() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (pongMode == PONG_MODE_HOST) {
    display.setCursor(15, 10);
    display.println("=== HOST ===");
    display.setCursor(5, 25);
    display.println("En attente d'un");
    display.setCursor(5, 35);
    display.println("joueur...");
  } else {
    display.setCursor(10, 10);
    display.println("=== CLIENT ===");
    display.setCursor(5, 25);
    display.println("Recherche d'une");
    display.setCursor(5, 35);
    display.println("partie...");
  }
  
  // Animation de recherche
  display.setCursor(50, 48);
  int dots = (millis() / 300) % 4;
  for (int i = 0; i < dots; i++) {
    display.print(".");
  }
  
  // Instructions
  display.setCursor(15, 56);
  display.print("BTN1: Annuler");
  
  // Afficher MAC locale
  // display.setCursor(0, 56);
  // display.print(WiFi.macAddress());
}

void PongGame::drawGameOver(int winner) {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  if (pongMode == PONG_MODE_LOCAL) {
    display.setCursor(10, 10);
    display.print("Player ");
    display.print(winner);
  } else {
    display.setCursor(20, 10);
    if ((pongMode == PONG_MODE_HOST && winner == 1) ||
        (pongMode == PONG_MODE_CLIENT && winner == 2)) {
      display.print("VICTOIRE!");
    } else {
      display.print("DEFAITE");
    }
  }
  
  display.setCursor(25, 32);
  display.println("WINS!");
  
  // Score final
  display.setTextSize(1);
  display.setCursor(30, 48);
  display.print("Score: ");
  display.print(score1);
  display.print(" - ");
  display.print(score2);
  
  display.setCursor(15, 56);
  display.println("BTN2: Menu");
}

void PongGame::drawMultiplayerMenu() {
  // Non utilisé - intégré dans drawMenu()
}

void PongGame::updateMultiplayerMenu() {
  // Non utilisé - intégré dans update()
}

bool PongGame::shouldReturnToMenu() {
  return requestReturnToMenu;
}