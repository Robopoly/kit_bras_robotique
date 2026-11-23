#include "doom_game.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include "../common/config.h"

static const int DEAD_ZONE_CENTER = 300;

// ============================================================
// Singleton et constantes statiques
// ============================================================
DoomGame* DoomGame::instance = nullptr;
const int DoomGame::MAX_ENEMIES;

// ============================================================
// Constructeur
// ============================================================
DoomGame::DoomGame(Adafruit_SSD1306* disp)
    : display(disp),
      doomMode(DOOM_MODE_SOLO),
      playerX(3.5f), playerY(3.5f), playerAngle(0.0f),
      enemiesAlive(0), totalKills(0),
      currentLevel(1), totalLevelsCompleted(0),
      isShooting(false), shootStartTime(0), lastButtonAState(false),
      exitX(6.5f), exitY(6.5f), levelComplete(false),
      gameState(0), gameRunning(false),
      lastUpdateTime(0), gameStartTime(0), lastButtonBState(false),
      menuSelection(0), lastMenuBtnState(false), lastMenuBtnTime(0), lastJoy1VRaw(2048),
      espNowInitialized(false), peerConnected(false), lastPeerMessage(0),
      lastStateSent(0), lastDiscoverySent(0),
      opponentX(1.5f), opponentY(1.5f), opponentAngle(0.0f),
      opponentHealth(DEATHMATCH_HP), myHealth(DEATHMATCH_HP),
      myKills(0), opponentKills(0),
      newStateReceived(false), newShootReceived(false),
      opponentShooting(false), opponentShootTime(0) {
    instance = this;
    memset(peerMAC, 0, 6);
    initMap();
    spawnEnemies();
}

// ============================================================
// init() – appelé à chaque entrée depuis le menu principal
// ============================================================
void DoomGame::init() {
    instance = this;

    // Nettoyer ESP-NOW si actif
    if (espNowInitialized) deinitESPNow();

    doomMode    = DOOM_MODE_SOLO;
    gameState   = 0;   // 0 = sélection du mode
    gameRunning = true;
    menuSelection   = 0;
    lastMenuBtnState = false;
    lastMenuBtnTime  = millis();
    lastJoy1VRaw     = 2048;

    currentLevel        = 1;
    totalLevelsCompleted = 0;
    playerX = 3.5f; playerY = 3.5f; playerAngle = 0.0f;
    lastButtonBState = false;
    lastButtonAState = false;
    isShooting       = false;
    levelComplete    = false;
    myHealth         = DEATHMATCH_HP;
    myKills          = 0;
    opponentKills    = 0;
    opponentHealth   = DEATHMATCH_HP;
    peerConnected    = false;
    memset(peerMAC, 0, 6);
    newStateReceived = false;
    newShootReceived = false;
    opponentShooting = false;

    initMap();
    spawnEnemies();
}

void DoomGame::start() {
    playerX = 3.5f; playerY = 3.5f; playerAngle = 0.0f;
    gameRunning   = true;
    lastUpdateTime = millis();
}

// ============================================================
// update() – dispatcher principal
// ============================================================
void DoomGame::update(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB) {
    if (!gameRunning) return;

    unsigned long now = millis();

    // Les valeurs sont déjà centrées (−2048..+2048) par main.cpp
    int v1 = joy1V;
    int h1 = joy1H;
    int v2 = joy2V;
    int h2 = joy2H;
    const int DEAD = 300;
    if (v1 > -DEAD && v1 < DEAD) v1 = 0;
    if (h1 > -DEAD && h1 < DEAD) h1 = 0;
    if (v2 > -DEAD && v2 < DEAD) v2 = 0;
    if (h2 > -DEAD && h2 < DEAD) h2 = 0;

    switch (gameState) {
        case 0:  // Sélection Solo / Deathmatch
            updateModeMenu(joy1V + 2048, buttonA, buttonB);  // reconvertir en brut pour la navigation menu
            break;

        case 1:  // Attente adversaire (deathmatch)
            if (now - lastUpdateTime < 33) return;
            lastUpdateTime = now;
            updateWaiting(buttonA);
            break;

        case 2:  // En jeu
            if (now - lastUpdateTime < 33) return;
            lastUpdateTime = now;
            if (doomMode == DOOM_MODE_SOLO) {
                handleInput(v1, h1, v2, h2, buttonA, buttonB);
            } else {
                updateDeathmatch(v1, h1, v2, h2, buttonA, buttonB);
            }
            break;

        case 3:  // Game over / Victoire
            // Appuyer sur l'un ou l'autre bouton pour quitter
            if ((buttonA && !lastButtonAState) || (buttonB && !lastButtonBState)) {
                gameRunning = false;
            }
            lastButtonAState = buttonA;
            lastButtonBState = buttonB;
            break;
    }
}

// ============================================================
// draw() – dispatcher principal
// ============================================================
void DoomGame::draw() {
    if (!gameRunning) return;

    display->clearDisplay();

    switch (gameState) {
        case 0: drawModeMenu();  break;
        case 1: drawWaiting();   break;
        case 2:
            if (doomMode == DOOM_MODE_SOLO) {
                drawWalls(); drawMiniMap(); drawWeapon(); drawHUD();
            } else {
                drawDeathmatch();
            }
            break;
        case 3:
            // Affichage fin de partie (géré dans les méthodes qui ont déclenché gameState=3)
            break;
    }

    display->display();
}

bool DoomGame::isRunning()          { return gameRunning; }
bool DoomGame::shouldReturnToMenu() { return !gameRunning; }

// ============================================================
// MENU SÉLECTION DE MODE
// ============================================================
void DoomGame::updateModeMenu(int joy1V, bool buttonA, bool buttonB) {
    unsigned long now = millis();

    // Navigation joystick (valeur brute ADC 0-4095)
    bool moved = false;
    if (joy1V < 1000 && lastJoy1VRaw >= 1000) {
        menuSelection = (menuSelection + 1) % 2;
        moved = true;
    } else if (joy1V > 3000 && lastJoy1VRaw <= 3000) {
        menuSelection = (menuSelection + 1) % 2;
        moved = true;
    }
    lastJoy1VRaw = joy1V;

    // Bouton B : confirmer
    bool btnB = buttonB;
    bool btnBEdge = btnB && !lastMenuBtnState && (now - lastMenuBtnTime > 300);
    if (btnBEdge) {
        lastMenuBtnState = btnB;
        lastMenuBtnTime  = now;
        if (menuSelection == 0) {
            // Solo
            doomMode    = DOOM_MODE_SOLO;
            gameState   = 2;
            gameStartTime = millis();
            lastButtonBState = true;  // Absorber le bouton courant
            lastButtonAState = false;
            return;
        } else {
            // Deathmatch : lancer la recherche d'adversaire
            doomMode    = DOOM_MODE_DEATHMATCH;
            gameState   = 1;
            myHealth    = DEATHMATCH_HP;
            myKills     = 0;
            opponentKills = 0;
            peerConnected = false;
            initESPNow();
            lastDiscoverySent = 0;
            return;
        }
    }
    lastMenuBtnState = btnB;

    // Bouton A : quitter vers menu principal
    bool btnA = buttonA;
    if (btnA && !lastButtonAState && (now - lastMenuBtnTime > 300)) {
        gameRunning = false;
    }
    lastButtonAState = btnA;
}

void DoomGame::drawModeMenu() {
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(30, 2);
    display->print("DOOM");
    display->drawLine(0, 12, 127, 12, SSD1306_WHITE);

    const char* items[] = { "Mode Solo", "Deathmatch 1v1" };
    for (int i = 0; i < 2; i++) {
        int y = 18 + i * 14;
        if (i == menuSelection) {
            display->fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
            display->setTextColor(SSD1306_BLACK);
        } else {
            display->setTextColor(SSD1306_WHITE);
        }
        display->setCursor(4, y);
        display->print(items[i]);
    }
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 56);
    display->print("BtnA:Quitter BtnB:OK");
}

// ============================================================
// ATTENTE ADVERSAIRE
// ============================================================
void DoomGame::updateWaiting(bool buttonA) {
    unsigned long now = millis();

    // Bouton A : annuler
    if (buttonA && !lastButtonAState) {
        deinitESPNow();
        gameState = 0;
    }
    lastButtonAState = buttonA;

    // Broadcasts de découverte
    if (now - lastDiscoverySent > DISCOVERY_INTERVAL) {
        sendDiscovery();
        lastDiscoverySent = now;
    }

    // Traiter les messages reçus
    if (newStateReceived) {
        newStateReceived = false;
        // Un adversaire nous a envoyé son état → la partie est déjà lancée quelque part
        // On l'accepte comme pair et on démarre
    }
}

void DoomGame::drawWaiting() {
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(10, 10);
    display->print("Deathmatch");
    display->setCursor(5, 25);
    display->print("Recherche joueur...");

    // Animation dots
    unsigned long t = millis() / 500 % 4;
    for (unsigned long i = 0; i < t; i++) {
        display->drawCircle(50 + (int)i * 10, 45, 3, SSD1306_WHITE);
    }

    display->setCursor(10, 56);
    display->print("BtnA : Annuler");
}

// ============================================================
// UPDATE EN MODE DEATHMATCH
// ============================================================
void DoomGame::updateDeathmatch(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB) {
    unsigned long now = millis();

    // Période de grâce au démarrage : absorber les boutons flottants/tenus
    if (now - gameStartTime < 800) {
        lastButtonAState = buttonA;
        lastButtonBState = buttonB;
        return;
    }

    // Bouton A : quitter
    if (buttonA && !lastButtonAState) {
        if (espNowInitialized) deinitESPNow();
        gameRunning = false;
    }
    lastButtonAState = buttonA;

    // Déplacement du joueur (identique solo)
    if (joy1V < -DEAD_ZONE_CENTER) {
        // Reculer
        float newX = playerX - cos(playerAngle) * MOVE_SPEED;
        float newY = playerY - sin(playerAngle) * MOVE_SPEED;
        int mx = (int)newX, my = (int)newY;
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT && map[my][mx] == 0) {
            playerX = newX; playerY = newY;
        }
    } else if (joy1V > DEAD_ZONE_CENTER) {
        // Avancer
        float newX = playerX + cos(playerAngle) * MOVE_SPEED;
        float newY = playerY + sin(playerAngle) * MOVE_SPEED;
        int mx = (int)newX, my = (int)newY;
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT && map[my][mx] == 0) {
            playerX = newX; playerY = newY;
        }
    }
    if (joy1H < -DEAD_ZONE_CENTER) {
        float sa = playerAngle + PI / 2.0f;
        float newX = playerX + cos(sa) * MOVE_SPEED;
        float newY = playerY + sin(sa) * MOVE_SPEED;
        int mx = (int)newX, my = (int)newY;
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT && map[my][mx] == 0) {
            playerX = newX; playerY = newY;
        }
    } else if (joy1H > DEAD_ZONE_CENTER) {
        float sa = playerAngle - PI / 2.0f;
        float newX = playerX + cos(sa) * MOVE_SPEED;
        float newY = playerY + sin(sa) * MOVE_SPEED;
        int mx = (int)newX, my = (int)newY;
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT && map[my][mx] == 0) {
            playerX = newX; playerY = newY;
        }
    }
    // Rotation
    if (joy2H < -DEAD_ZONE_CENTER) { playerAngle += ROT_SPEED; }
    else if (joy2H > DEAD_ZONE_CENTER) { playerAngle -= ROT_SPEED; }
    while (playerAngle < -PI) playerAngle += 2 * PI;
    while (playerAngle > PI)  playerAngle -= 2 * PI;

    // Tir (bouton B)
    if (buttonB && !lastButtonBState) {
        // Animation locale
        isShooting    = true;
        shootStartTime = now;
        // Vérifier si l'adversaire est dans la ligne de mire
        if (peerConnected) {
            int hitEnemy = -1;
            float dist = castRay(playerAngle, false, &hitEnemy);
            // Distance adversaire
            float dx = opponentX - playerX;
            float dy = opponentY - playerY;
            float opDist = sqrt(dx*dx + dy*dy);
            // Angle vers adversaire
            float opAngle = atan2(dy, dx);
            float angleDiff = opAngle - playerAngle;
            while (angleDiff < -PI) angleDiff += 2*PI;
            while (angleDiff > PI)  angleDiff -= 2*PI;
            bool inFOV = fabs(angleDiff) < FOV / 2.0f;
            if (inFOV && opDist < dist && opDist < 10.0f) {
                // Touché !
                sendShootMsg();
            }
        }
    }
    lastButtonBState = buttonB;

    // Appliquer les messages reçus
    if (newStateReceived) {
        newStateReceived = false;
        opponentX     = receivedState.posX    / 100.0f;
        opponentY     = receivedState.posY    / 100.0f;
        opponentAngle = receivedState.angle   / 1000.0f;
        opponentHealth = receivedState.health;
        lastPeerMessage = now;
        // Si l'adversaire envoie son état en jeu et qu'on était en attente, démarrer
        if (gameState == 1) {
            gameState = 2;
            peerConnected = true;
            gameStartTime = now;
            lastButtonBState = true;
            lastButtonAState = false;
        }
    }
    if (newShootReceived) {
        newShootReceived = false;
        // Vérifier si notre position était dans la ligne de tir adverse
        float dx = playerX - receivedShoot.originX / 100.0f;
        float dy = playerY - receivedShoot.originY / 100.0f;
        float shootAngle = receivedShoot.angle / 1000.0f;
        float dirX = cos(shootAngle), dirY = sin(shootAngle);
        // Produit scalaire -> projection sur direction de tir
        float dot = dx * dirX + dy * dirY;
        float perpX = dx - dot * dirX;
        float perpY = dy - dot * dirY;
        float perpDist = sqrt(perpX*perpX + perpY*perpY);
        if (dot > 0 && perpDist < 0.5f && dot < 12.0f) {
            // Nous sommes touchés
            myHealth--;
            opponentKills += (myHealth <= 0) ? 1 : 0;
            uint8_t rem = (myHealth < 0) ? 0 : (uint8_t)myHealth;
            sendHitMsg(1, rem);
            if (myHealth <= 0) {
                // Respawn
                playerX = 3.5f; playerY = 3.5f; playerAngle = 0.0f;
                myHealth = DEATHMATCH_HP;
                // Vérifier victoire adverse
                if (opponentKills >= KILLS_TO_WIN) {
                    gameState = 3;
                    display->clearDisplay();
                    display->setTextSize(2);
                    display->setCursor(10, 20);
                    display->print("DEFAITE");
                    display->setTextSize(1);
                    display->setCursor(20, 45);
                    display->print("Appuyez pour quit");
                    display->display();
                    delay(100);
                }
            }
        }
    }
    // Appliquer confirmation de hit sur adversaire (via onDataReceived → receivedState.health)
    // La santé adverse est déjà mise à jour via newStateReceived ci-dessus.

    // Vérifier si j'ai gagné par kills
    if (myKills >= KILLS_TO_WIN && gameState == 2) {
        gameState = 3;
        display->clearDisplay();
        display->setTextSize(2);
        display->setCursor(20, 20);
        display->print("VICTOIRE");
        display->setTextSize(1);
        display->setCursor(20, 45);
        display->print("Appuyez pour quit");
        display->display();
        delay(100);
    }

    // Timeout peer
    if (peerConnected && (now - lastPeerMessage > PEER_TIMEOUT)) {
        peerConnected = false;
        display->clearDisplay();
        display->setTextSize(1);
        display->setCursor(5, 25);
        display->print("Adversaire deconnecte");
        display->display();
        delay(1500);
        deinitESPNow();
        gameRunning = false;
        return;
    }

    // Envoi périodique de l'état
    if (peerConnected && (now - lastStateSent > STATE_SEND_INTERVAL)) {
        sendPlayerState();
        lastStateSent = now;
    }
    // Broadcasts de découverte si pas encore connecté
    if (!peerConnected && gameState == 1) {
        if (now - lastDiscoverySent > DISCOVERY_INTERVAL) {
            sendDiscovery();
            lastDiscoverySent = now;
        }
    }
}

void DoomGame::drawDeathmatch() {
    // Injecter l'adversaire dans le slot 0 du tableau enemies pour que
    // drawWalls() le rende automatiquement avec le raycasting existant
    bool opponentInjected = false;
    if (peerConnected) {
        enemies[0].x     = opponentX;
        enemies[0].y     = opponentY;
        enemies[0].alive = true;
        enemies[0].health = 1;
        opponentInjected  = true;
        enemiesAlive      = 1;
    }

    drawWalls();

    // Retirer l'ennemi injecté pour ne pas polluer la logique solo
    if (opponentInjected) {
        enemies[0].alive = false;
        enemiesAlive     = 0;
    }

    // Dessiner l'arme
    drawWeapon();

    // HUD deathmatch
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    display->print("HP:");
    display->print(myHealth);
    display->print(" K:");
    display->print(myKills);

    display->setCursor(70, 0);
    display->print("ADV K:");
    display->print(opponentKills);

    // Indicateur connexion
    if (!peerConnected) {
        display->setCursor(40, 28);
        display->print("Attente...");
    }

    // Viseur
    display->drawPixel(64, 32, SSD1306_WHITE);
    display->drawPixel(63, 32, SSD1306_WHITE);
    display->drawPixel(65, 32, SSD1306_WHITE);
    display->drawPixel(64, 31, SSD1306_WHITE);
    display->drawPixel(64, 33, SSD1306_WHITE);
}

// ============================================================
// ESP-NOW
// ============================================================
void DoomGame::initESPNow() {
    if (espNowInitialized) return;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_send_cb(onDataSentStatic);
    esp_now_register_recv_cb(onDataReceivedStatic);
    // Enregistrer l'adresse broadcast
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
        esp_now_add_peer(&peerInfo);
    }
    espNowInitialized = true;
}

void DoomGame::deinitESPNow() {
    if (!espNowInitialized) return;
    if (esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_del_peer(BROADCAST_MAC);
    if (peerConnected && esp_now_is_peer_exist(peerMAC)) esp_now_del_peer(peerMAC);
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    espNowInitialized = false;
    peerConnected = false;
}

void DoomGame::addPeer(const uint8_t* mac) {
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void DoomGame::removePeer(const uint8_t* mac) {
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
}

void DoomGame::sendPlayerState() {
    if (!espNowInitialized) return;
    DoomPlayerState msg = {};
    msg.msgType    = DOOM_MSG_PLAYER_STATE;
    msg.posX       = (int16_t)(playerX * 100);
    msg.posY       = (int16_t)(playerY * 100);
    msg.angle      = (int16_t)(playerAngle * 1000);
    msg.health     = (uint8_t)(myHealth < 0 ? 0 : myHealth);
    msg.isShooting = isShooting ? 1 : 0;
    msg.timestamp  = millis();
    esp_now_send(peerMAC, (uint8_t*)&msg, sizeof(msg));
}

void DoomGame::sendShootMsg() {
    if (!espNowInitialized) return;
    DoomShootMsg msg = {};
    msg.msgType  = DOOM_MSG_SHOOT;
    msg.originX  = (int16_t)(playerX * 100);
    msg.originY  = (int16_t)(playerY * 100);
    msg.angle    = (int16_t)(playerAngle * 1000);
    msg.timestamp = millis();
    esp_now_send(peerMAC, (uint8_t*)&msg, sizeof(msg));
}

void DoomGame::sendHitMsg(uint8_t damage, uint8_t remainingHP) {
    if (!espNowInitialized) return;
    DoomHitMsg msg = {};
    msg.msgType    = DOOM_MSG_HIT;
    msg.damage     = damage;
    msg.remainingHP = remainingHP;
    msg.timestamp  = millis();
    esp_now_send(peerMAC, (uint8_t*)&msg, sizeof(msg));
}

void DoomGame::sendDiscovery() {
    if (!espNowInitialized) return;
    uint8_t buf[2] = { DOOM_MSG_DISCOVERY, 0x01 };
    esp_now_send(BROADCAST_MAC, buf, 2);
}

void DoomGame::onDataSentStatic(const uint8_t* mac, esp_now_send_status_t status) {
    if (instance) instance->onDataSent(mac, status);
}

void DoomGame::onDataReceivedStatic(const uint8_t* mac, const uint8_t* data, int len) {
    if (instance && len > 0) instance->onDataReceived(mac, data, len);
}

void DoomGame::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    // Pas utilisé pour l'instant
}

void DoomGame::onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < 1) return;
    uint8_t msgType = data[0];

    switch (msgType) {
        case DOOM_MSG_DISCOVERY:
        case DOOM_MSG_DISCOVERY_ACK: {
            if (!peerConnected) {
                memcpy(peerMAC, mac, 6);
                addPeer(peerMAC);
                peerConnected   = true;
                lastPeerMessage = millis();
                // Si on était en attente, démarrer la partie
                if (gameState == 1) {
                    // Réinitialiser la carte et les positions de départ
                    initMap();
                    playerX = 1.5f; playerY = 1.5f; playerAngle = 0.0f;
                    opponentX = 6.5f; opponentY = 6.5f;
                    gameState     = 2;
                    gameStartTime = millis();
                    lastButtonBState = true;
                    lastButtonAState = false;
                }
                // Répondre si discovery (pas ACK)
                if (msgType == DOOM_MSG_DISCOVERY) {
                    uint8_t ack[2] = { DOOM_MSG_DISCOVERY_ACK, 0x01 };
                    esp_now_send(peerMAC, ack, 2);
                }
            }
            break;
        }
        case DOOM_MSG_PLAYER_STATE: {
            if (len >= (int)sizeof(DoomPlayerState)) {
                memcpy(&receivedState, data, sizeof(DoomPlayerState));
                newStateReceived = true;
                lastPeerMessage  = millis();
            }
            break;
        }
        case DOOM_MSG_SHOOT: {
            if (len >= (int)sizeof(DoomShootMsg)) {
                memcpy(&receivedShoot, data, sizeof(DoomShootMsg));
                newShootReceived = true;
            }
            break;
        }
        case DOOM_MSG_HIT: {
            if (len >= (int)sizeof(DoomHitMsg)) {
                DoomHitMsg hitMsg;
                memcpy(&hitMsg, data, sizeof(DoomHitMsg));
                // L'adversaire confirme qu'il nous a touché → incrémenter nos kills
                myKills++;
                // Mettre à jour sa santé selon sa propre donnée
                opponentHealth = hitMsg.remainingHP;
            }
            break;
        }
        case DOOM_MSG_GAME_OVER: {
            // L'adversaire quitte
            peerConnected = false;
            break;
        }
    }
}

// ============================================================
// MÉTHODES SOLO (inchangées)
// ============================================================

void DoomGame::initMap() {
    if (currentLevel == 1) {
        // Niveau 1 : map simple fixe
        int mapData[MAP_HEIGHT][MAP_WIDTH] = {
            {1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1}
        };
        
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                map[y][x] = mapData[y][x];
            }
        }
    } else {
        // Niveaux suivants : génération aléatoire
        generateRandomMap();
    }
}

void DoomGame::generateRandomMap() {
    // Initialiser avec des sols partout
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) {
                map[y][x] = 1;  // Bords = murs
            } else {
                map[y][x] = 0;  // Intérieur = sol
            }
        }
    }
    
    // Plus le niveau est élevé, plus il y a de murs
    // Niveau 2: 30%, Niveau 3: 40%, Niveau 4: 50%, Niveau 5+: 60%
    int wallPercentage = min(30 + (currentLevel - 2) * 10, 60);
    
    // Remplir aléatoirement avec des murs selon le pourcentage
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            // Ne pas toucher aux zones de spawn et sortie
            bool isSpawnArea = (x >= 2 && x <= 4 && y >= 2 && y <= 4);
            bool isExitArea = (x >= 5 && x <= 6 && y >= 5 && y <= 6);
            
            if (!isSpawnArea && !isExitArea) {
                if (rand() % 100 < wallPercentage) {
                    map[y][x] = 1;
                }
            }
        }
    }
    
    // S'assurer que la zone de départ est totalement libre (3x3)
    for (int y = 2; y <= 4; y++) {
        for (int x = 2; x <= 4; x++) {
            map[y][x] = 0;
        }
    }
    
    // S'assurer que la zone d'arrivée est libre (2x2)
    map[6][6] = 0;
    map[6][5] = 0;
    map[5][6] = 0;
    map[5][5] = 0;
    
    // Assurer la connectivité de la map
    ensureMapConnectivity();
}

// Flood fill pour garantir que toutes les zones sont accessibles depuis le spawn
void DoomGame::ensureMapConnectivity() {
    // Tableau pour marquer les cases visitées
    bool visited[MAP_HEIGHT][MAP_WIDTH];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visited[y][x] = false;
        }
    }
    
    // Stack pour le flood fill (éviter récursion)
    int stack[100][2];  // [index][x/y]
    int stackSize = 0;
    
    // Démarrer depuis la position du joueur (spawn)
    int startX = 3, startY = 3;
    stack[stackSize][0] = startX;
    stack[stackSize][1] = startY;
    stackSize++;
    visited[startY][startX] = true;
    
    // Flood fill itératif
    while (stackSize > 0) {
        stackSize--;
        int currentX = stack[stackSize][0];
        int currentY = stack[stackSize][1];
        
        // Vérifier les 4 directions
        int dx[] = {0, 1, 0, -1};
        int dy[] = {-1, 0, 1, 0};
        
        for (int i = 0; i < 4; i++) {
            int newX = currentX + dx[i];
            int newY = currentY + dy[i];
            
            // Vérifier limites
            if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT) {
                // Si c'est un sol et pas encore visité
                if (map[newY][newX] == 0 && !visited[newY][newX]) {
                    visited[newY][newX] = true;
                    if (stackSize < 100) {  // Sécurité
                        stack[stackSize][0] = newX;
                        stack[stackSize][1] = newY;
                        stackSize++;
                    }
                }
            }
        }
    }
    
    // Vérifier que la sortie est accessible
    if (!visited[6][6] && !visited[6][5] && !visited[5][6] && !visited[5][5]) {
        // Ouvrir un chemin vers la sortie depuis le spawn
        for (int x = 4; x <= 5; x++) {
            map[5][x] = 0;
            visited[5][x] = true;
        }
    }
    
    // Pour toutes les cases sol non visitées, ouvrir un passage
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            if (map[y][x] == 0 && !visited[y][x]) {
                // Trouver la direction vers la zone visitée la plus proche
                bool connected = false;
                
                // Essayer d'ouvrir un passage dans chaque direction
                int dx[] = {0, 1, 0, -1};
                int dy[] = {-1, 0, 1, 0};
                
                for (int dir = 0; dir < 4 && !connected; dir++) {
                    int checkX = x + dx[dir];
                    int checkY = y + dy[dir];
                    
                    // Si on trouve une case visitée adjacente, ouvrir le mur entre les deux
                    if (checkX > 0 && checkX < MAP_WIDTH - 1 && 
                        checkY > 0 && checkY < MAP_HEIGHT - 1 && 
                        visited[checkY][checkX]) {
                        
                        // Ouvrir le passage
                        map[y][x] = 0;
                        visited[y][x] = true;
                        connected = true;
                        
                        // Relancer un mini flood fill depuis cette position
                        int miniStack[50][2];
                        int miniStackSize = 0;
                        miniStack[miniStackSize][0] = x;
                        miniStack[miniStackSize][1] = y;
                        miniStackSize++;
                        
                        while (miniStackSize > 0) {
                            miniStackSize--;
                            int cx = miniStack[miniStackSize][0];
                            int cy = miniStack[miniStackSize][1];
                            
                            for (int i = 0; i < 4; i++) {
                                int nx = cx + dx[i];
                                int ny = cy + dy[i];
                                
                                if (nx > 0 && nx < MAP_WIDTH - 1 && 
                                    ny > 0 && ny < MAP_HEIGHT - 1 &&
                                    map[ny][nx] == 0 && !visited[ny][nx]) {
                                    
                                    visited[ny][nx] = true;
                                    if (miniStackSize < 50) {
                                        miniStack[miniStackSize][0] = nx;
                                        miniStack[miniStackSize][1] = ny;
                                        miniStackSize++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void DoomGame::nextLevel() {
    currentLevel++;
    totalLevelsCompleted++;
    
    // Réinitialiser le niveau
    playerX = 3.5f;
    playerY = 3.5f;
    playerAngle = 0.0f;
    levelComplete = false;
    
    initMap();
    spawnEnemies();
    
    // Afficher message de niveau APRÈS avoir placé les ennemis
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(10, 15);
    display->print("LEVEL ");
    display->print(currentLevel);
    display->setTextSize(1);
    display->setCursor(5, 40);
    display->print("Enemies: ");
    display->print(enemiesAlive);  // Afficher le nombre réel d'ennemis placés
    display->display();
    delay(2000);
}

void DoomGame::spawnEnemies() {
    // Nombre d'ennemis basé sur le niveau (min 3, max 10)
    int numEnemies = min(3 + currentLevel, MAX_ENEMIES);
    
    // Réinitialiser tous les ennemis
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].alive = false;
        enemies[i].health = 1;  // 1 coup pour tuer
    }
    
    // Faire un flood fill pour identifier les zones accessibles
    bool visited[MAP_HEIGHT][MAP_WIDTH];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visited[y][x] = false;
        }
    }
    
    // Stack pour le flood fill
    int stack[100][2];
    int stackSize = 0;
    
    // Démarrer depuis la position du joueur (spawn)
    int startX = (int)playerX;
    int startY = (int)playerY;
    stack[stackSize][0] = startX;
    stack[stackSize][1] = startY;
    stackSize++;
    visited[startY][startX] = true;
    
    // Flood fill itératif
    while (stackSize > 0) {
        stackSize--;
        int currentX = stack[stackSize][0];
        int currentY = stack[stackSize][1];
        
        // Vérifier les 4 directions
        int dx[] = {0, 1, 0, -1};
        int dy[] = {-1, 0, 1, 0};
        
        for (int i = 0; i < 4; i++) {
            int newX = currentX + dx[i];
            int newY = currentY + dy[i];
            
            if (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT) {
                if (map[newY][newX] == 0 && !visited[newY][newX]) {
                    visited[newY][newX] = true;
                    if (stackSize < 100) {
                        stack[stackSize][0] = newX;
                        stack[stackSize][1] = newY;
                        stackSize++;
                    }
                }
            }
        }
    }
    
    // Créer une liste de positions libres valides ET accessibles
    float validPositions[36][2];  // Max 6x6 = 36 positions internes
    int numValidPositions = 0;
    
    // Scanner toute la map pour trouver les positions libres ET accessibles
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            // Vérifier que c'est un sol ET qu'il est accessible (visité par flood fill)
            if (map[y][x] == 0 && visited[y][x]) {
                float posX = x + 0.5f;
                float posY = y + 0.5f;
                
                // Vérifier que ce n'est pas trop proche du joueur
                float dx = posX - playerX;
                float dy = posY - playerY;
                float dist = sqrt(dx*dx + dy*dy);
                
                if (dist > 2.0f && numValidPositions < 36) {
                    validPositions[numValidPositions][0] = posX;
                    validPositions[numValidPositions][1] = posY;
                    numValidPositions++;
                }
            }
        }
    }
    
    // Placer les ennemis aléatoirement parmi les positions valides
    enemiesAlive = 0;
    for (int i = 0; i < numEnemies && i < numValidPositions; i++) {
        // Choisir une position aléatoire non utilisée
        int posIndex = rand() % numValidPositions;
        
        enemies[i].x = validPositions[posIndex][0];
        enemies[i].y = validPositions[posIndex][1];
        enemies[i].alive = true;
        enemies[i].health = 1;
        enemiesAlive++;
        
        // Retirer cette position de la liste pour éviter les doublons
        validPositions[posIndex][0] = validPositions[numValidPositions - 1][0];
        validPositions[posIndex][1] = validPositions[numValidPositions - 1][1];
        numValidPositions--;
    }
    
    totalKills = 0;
}

float DoomGame::castRay(float angle, bool checkEnemies, int* hitEnemyIndex) {
    // Normaliser l'angle entre -PI et PI
    while (angle < -PI) angle += 2 * PI;
    while (angle > PI) angle -= 2 * PI;
    
    float rayDirX = cos(angle);
    float rayDirY = sin(angle);
    
    float rayX = playerX;
    float rayY = playerY;
    float stepSize = 0.02f;
    float distance = 0.0f;
    
    if (hitEnemyIndex) *hitEnemyIndex = -1;
    
    // Lancer le rayon jusqu'à toucher un mur (max 20 unités)
    for (int i = 0; i < 1000; i++) {
        rayX += rayDirX * stepSize;
        rayY += rayDirY * stepSize;
        distance += stepSize;
        
        // Vérifier collision avec ennemis (avant les murs)
        if (checkEnemies && hitEnemyIndex) {
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (enemies[e].alive) {
                    float dx = rayX - enemies[e].x;
                    float dy = rayY - enemies[e].y;
                    float dist = sqrt(dx*dx + dy*dy);
                    if (dist < 0.3f) {  // Rayon de l'ennemi
                        *hitEnemyIndex = e;
                        return distance;
                    }
                }
            }
        }
        
        int mapX = (int)rayX;
        int mapY = (int)rayY;
        
        // Vérifier les limites et les collisions
        if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
            return distance;
        }
        
        if (map[mapY][mapX] == 1) {
            return distance;
        }
        
        if (distance > 20.0f) {
            return distance;
        }
    }
    
    return distance;
}

void DoomGame::handleInput(int joy1V, int joy1H, int joy2V, int joy2H, bool buttonA, bool buttonB) {
    // Ignorer les inputs pendant les 500 premières ms (debouncing du bouton de sélection)
    unsigned long currentTime = millis();
    if (currentTime - gameStartTime < 500) {
        lastButtonBState = buttonB;  // Mise à jour de l'état même pendant la période de grâce
        lastButtonAState = buttonA;
        return;
    }
    
    // Bouton B pour tirer - avec debouncing (détection du front montant)
    if (buttonB && !lastButtonBState) {
        shoot();
    }
    lastButtonBState = buttonB;
    
    // Bouton A pour quitter - avec debouncing (détection du front montant)
    // Quitter SEULEMENT si le bouton vient d'être enfoncé (transition false -> true)
    if (buttonA && !lastButtonAState) {
        gameRunning = false;
    }
    lastButtonAState = buttonA;
    
    // JOY1 Vertical pour avancer/reculer
    if (joy1V < -300) {
        // Joystick 1 vers le haut = RECULER
        float newX = playerX - cos(playerAngle) * MOVE_SPEED;
        float newY = playerY - sin(playerAngle) * MOVE_SPEED;
        
        int mapX = (int)newX;
        int mapY = (int)newY;
        
        if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
            if (map[mapY][mapX] == 0) {
                playerX = newX;
                playerY = newY;
                
                // Vérifier victoire après déplacement
                checkVictory();
            }
        }
    } else if (joy1V > 300) {
        // Joystick 1 vers le bas = AVANCER
        float newX = playerX + cos(playerAngle) * MOVE_SPEED;
        float newY = playerY + sin(playerAngle) * MOVE_SPEED;
        
        int mapX = (int)newX;
        int mapY = (int)newY;
        
        if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
            if (map[mapY][mapX] == 0) {
                playerX = newX;
                playerY = newY;
                
                // Vérifier victoire après déplacement
                checkVictory();
            }
        }
    }
    
    // JOY1 Horizontal pour translation gauche/droite (strafe) - INVERSÉ
    if (joy1H < -300) {
        // Joystick 1 vers la gauche = TRANSLATION À DROITE
        // Se déplacer perpendiculairement à la direction (90 degrés à droite)
        float strafeAngle = playerAngle + PI / 2.0f;
        float newX = playerX + cos(strafeAngle) * MOVE_SPEED;
        float newY = playerY + sin(strafeAngle) * MOVE_SPEED;
        
        int mapX = (int)newX;
        int mapY = (int)newY;
        
        if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
            if (map[mapY][mapX] == 0) {
                playerX = newX;
                playerY = newY;
            }
        }
    } else if (joy1H > 300) {
        // Joystick 1 vers la droite = TRANSLATION À GAUCHE
        // Se déplacer perpendiculairement à la direction (90 degrés à gauche)
        float strafeAngle = playerAngle - PI / 2.0f;
        float newX = playerX + cos(strafeAngle) * MOVE_SPEED;
        float newY = playerY + sin(strafeAngle) * MOVE_SPEED;
        
        int mapX = (int)newX;
        int mapY = (int)newY;
        
        if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
            if (map[mapY][mapX] == 0) {
                playerX = newX;
                playerY = newY;
            }
        }
    }
    
    // JOY2 pour orientation (rotation) - INVERSÉ
    // JOY2 Horizontal pour tourner gauche/droite
    if (joy2H < -300) {
        // Joystick 2 vers la gauche = TOURNER À DROITE
        playerAngle += ROT_SPEED;
    } else if (joy2H > 300) {
        // Joystick 2 vers la droite = TOURNER À GAUCHE
        playerAngle -= ROT_SPEED;
    }
    
    // JOY2 Vertical pour orientation haut/bas (si souhaité pour regarder haut/bas)
    // Pour l'instant on peut l'ignorer ou l'utiliser pour d'autres fonctions
    // if (joy2V < -300) {
    //     // Regarder en haut
    // } else if (joy2V > 300) {
    //     // Regarder en bas
    // }
    
    // Normaliser l'angle
    while (playerAngle < -PI) playerAngle += 2 * PI;
    while (playerAngle > PI) playerAngle -= 2 * PI;
}

void DoomGame::drawWalls() {
    // Dessiner la vue 3D en raycasting
    for (int col = 0; col < NUM_RAYS; col++) {
        // Calculer l'angle du rayon
        float rayAngle = playerAngle - (FOV / 2.0f) + (FOV * col / (float)NUM_RAYS);
        
        // Lancer le rayon avec détection des ennemis
        int hitEnemyIndex = -1;
        float distance = castRay(rayAngle, true, &hitEnemyIndex);
        
        // Corriger la distorsion fish-eye
        float angleDiff = rayAngle - playerAngle;
        distance = distance * cos(angleDiff);
        
        // Calculer la hauteur du mur à l'écran
        float wallHeight = 0;
        if (distance > 0.1f) {
            wallHeight = (64.0f / distance) * 0.5f;
        }
        
        if (wallHeight > 32) wallHeight = 32;
        
        // Calculer les coordonnées y du mur
        int wallTop = 32 - (int)wallHeight;
        int wallBottom = 32 + (int)wallHeight;
        
        if (wallTop < 0) wallTop = 0;
        if (wallBottom > 63) wallBottom = 63;
        
        // Dessiner la colonne de mur
        int screenX = col * 128 / NUM_RAYS;
        int screenXNext = (col + 1) * 128 / NUM_RAYS;
        
        for (int x = screenX; x < screenXNext; x++) {
            // Dessiner le plafond (noir, pas besoin de dessiner)
            
            // Dessiner le mur ou l'ennemi
            if (hitEnemyIndex >= 0) {
                // C'est un ennemi - dessiner avec pattern différent
                for (int y = wallTop; y <= wallBottom; y++) {
                    if ((x + y) % 2 == 0) {  // Pattern pour différencier des murs
                        display->drawPixel(x, y, SSD1306_WHITE);
                    }
                }
            } else {
                // C'est un mur - dessiner normalement
                for (int y = wallTop; y <= wallBottom; y++) {
                    display->drawPixel(x, y, SSD1306_WHITE);
                }
            }
            
            // Dessiner le sol (gris foncé - quelques pixels espacés)
            for (int y = wallBottom + 1; y < 64; y += 2) {
                if ((x + y) % 2 == 0) {
                    display->drawPixel(x, y, SSD1306_WHITE);
                }
            }
        }
    }
}

void DoomGame::drawMiniMap() {
    // Mini-map désactivée pour ne pas cacher le HUD
    // On garde juste un petit indicateur de direction
}

void DoomGame::shoot() {
    // Vérifier si un ennemi est dans le viseur (centre de l'écran)
    int hitEnemyIndex = -1;
    castRay(playerAngle, true, &hitEnemyIndex);
    
    if (hitEnemyIndex >= 0 && enemies[hitEnemyIndex].alive) {
        // Toucher l'ennemi
        enemies[hitEnemyIndex].health--;
        
        if (enemies[hitEnemyIndex].health <= 0) {
            // Ennemi tué
            enemies[hitEnemyIndex].alive = false;
            enemiesAlive--;
            totalKills++;
        }
    }
    
    // Activer l'animation de tir
    isShooting = true;
    shootStartTime = millis();
}

void DoomGame::drawWeapon() {
    // Dessiner un sprite d'arme simple en bas au centre
    int weaponX = 64 - 8;  // Centré
    int weaponY = 50;
    
    // Vérifier si on est en train de tirer (animation flash)
    bool showFlash = false;
    if (isShooting && (millis() - shootStartTime < SHOOT_ANIMATION_DURATION)) {
        showFlash = true;
    } else {
        isShooting = false;  // Fin de l'animation
    }
    
    if (showFlash) {
        // Flash du tir - dessiner un triangle élargi
        display->drawLine(weaponX + 8, weaponY, weaponX + 4, weaponY - 8, SSD1306_WHITE);
        display->drawLine(weaponX + 8, weaponY, weaponX + 12, weaponY - 8, SSD1306_WHITE);
        display->drawLine(weaponX + 4, weaponY - 8, weaponX + 12, weaponY - 8, SSD1306_WHITE);
    } else {
        // Sprite d'arme normal - simple rectangle pour le pistolet
        display->fillRect(weaponX + 6, weaponY, 4, 10, SSD1306_WHITE);
        display->fillRect(weaponX + 4, weaponY + 10, 8, 3, SSD1306_WHITE);
    }
}

void DoomGame::drawHUD() {
    // Afficher le niveau en haut à gauche
    display->setCursor(0, 0);
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->print("LV");
    display->print(currentLevel);
    display->print(" K:");
    display->print(totalKills);
    display->print("/");
    display->print(enemiesAlive + totalKills);  // Total ennemis du niveau
    
    // Si tous les ennemis sont morts, afficher "EXIT"
    if (enemiesAlive == 0) {
        display->setCursor(90, 0);
        display->print("EXIT");
    }
    
    // Viseur au centre
    display->drawPixel(64, 32, SSD1306_WHITE);
    display->drawPixel(63, 32, SSD1306_WHITE);
    display->drawPixel(65, 32, SSD1306_WHITE);
    display->drawPixel(64, 31, SSD1306_WHITE);
    display->drawPixel(64, 33, SSD1306_WHITE);
}

void DoomGame::checkVictory() {
    // Vérifier si tous les ennemis sont morts
    if (enemiesAlive > 0) return;
    
    // Vérifier si le joueur est proche de la sortie
    float dx = playerX - exitX;
    float dy = playerY - exitY;
    float distToExit = sqrt(dx*dx + dy*dy);
    
    if (distToExit < 0.5f && !levelComplete) {
        levelComplete = true;
        
        // Passer au niveau suivant
        nextLevel();
    }
}
