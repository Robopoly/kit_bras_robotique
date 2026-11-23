#include "robot_control.h"

// Instance statique pour les callbacks
RobotControl* RobotControl::instance = nullptr;

RobotControl::RobotControl() :
  espNowInitialized(false), robotConnected(false), lastCommandTime(0),
  lastStatusReceived(0), calibrated(false),
  lastMenuButtonTime(0), lastMenuBtn(HIGH), requestReturnToMenu(false),
  lastTransmission(0) {
  
  // Initialiser les angles par défaut (position neutre)
  servoAngles[0] = 90; // Base
  servoAngles[1] = 90; // Épaule
  servoAngles[2] = 90; // Coude
  servoAngles[3] = 90; // Poignet
  servoAngles[4] = 90; // Pince
  
  instance = this;
}

void RobotControl::init() {
  Serial.println("Robot Control: Initialisation");
  
  // Réinitialiser les variables
  robotConnected = false;
  requestReturnToMenu = false;
  lastMenuButtonTime = millis();
  lastMenuBtn = HIGH;
  calibrated = false;
  lastTransmission = 0;
  
  // Initialiser ESP-NOW
  initESPNow();
  
  Serial.println("Robot Control: Prêt");
  Serial.println("=== DEBUG ROBOT CONTROL ===");
  Serial.print("Angles initiaux: ");
  for(int i = 0; i < 5; i++) {
    Serial.print(servoAngles[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void RobotControl::initESPNow() {
  // Configurer WiFi en mode Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Déconnecter de tout réseau existant
  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Initialiser ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur initialisation ESP-NOW");
    return;
  }
  
  // Enregistrer les callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);
  
  // Ajouter le peer (bras robotique) avec interface spécifiée
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo)); // Initialiser à zéro
  memcpy(peerInfo.peer_addr, robotArmMAC, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA; // Spécifier l'interface WiFi Station
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Erreur ajout peer ESP-NOW");
    Serial.print("MAC peer tentée: ");
    for (int i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      Serial.printf("%02X", robotArmMAC[i]);
    }
    Serial.println();
    return;
  }
  
  espNowInitialized = true;
  Serial.println("ESP-NOW initialisé avec succès");
  Serial.print("Peer ajouté: ");
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    Serial.printf("%02X", robotArmMAC[i]);
  }
  Serial.println();
}

void RobotControl::update() {
  unsigned long currentTime = millis();
  
  // Vérifier retour au menu
  bool menuBtn = digitalRead(JOY1_BTN_PIN);
  if (menuBtn == LOW && lastMenuBtn == HIGH && 
      (currentTime - lastMenuButtonTime > 500)) {
    requestReturnToMenu = true;
    lastMenuButtonTime = currentTime;
    Serial.println("Retour au menu demandé");
    return;
  }
  lastMenuBtn = menuBtn;
  
  // Mettre à jour depuis les joysticks
  updateFromJoysticks();
  
  // Envoyer commande si nécessaire
  if (espNowInitialized && (currentTime - lastTransmission > TRANSMISSION_RATE)) {
    if (hasCommandChanged() || (currentTime - lastCommandTime > 200)) {
      sendCommand();
      lastTransmission = currentTime;
    }
  }
  
  // Vérifier la connexion (timeout)
  if (currentTime - lastStatusReceived > COMMAND_TIMEOUT) {
    robotConnected = false;
  }
}

void RobotControl::updateFromJoysticks() {
  unsigned long currentTime = millis();
  
  // Lecture des joysticks
  int joy1H = analogRead(JOY1_H_PIN);  // Horizontal joystick 1 (GPIO2)
  int joy1V = analogRead(JOY1_V);      // Vertical joystick 1 (GPIO1)
  int joy2H = analogRead(JOY2_H);      // Horizontal joystick 2 (GPIO4)
  int joy2V = analogRead(JOY2_V);      // Vertical joystick 2 (GPIO3)
  bool joy2Btn = digitalRead(JOYSTICK_BTN_PIN);  // Bouton joystick 2 pour la pince (GPIO6)
  
  // Debug joysticks (une fois par seconde)
  static unsigned long lastJoyDebug = 0;
  if (currentTime - lastJoyDebug > 1000) {
    Serial.print("Joy1: [");
    Serial.print(joy1H); Serial.print(",");
    Serial.print(joy1V); Serial.print("] Joy2: [");
    Serial.print(joy2H); Serial.print(",");
    Serial.print(joy2V); Serial.print("] Joy2Btn:");
    Serial.println(joy2Btn ? "1" : "0");
    lastJoyDebug = currentTime;
  }
  
  // Calibration automatique du centre
  if (!calibrated) {
    joystickCenter = (joy1H + joy1V + joy2H + joy2V) / 4;
    calibrated = true;
    Serial.print("Calibration joystick center: ");
    Serial.println(joystickCenter);
  }
  
  // Contrôle simultané de tous les axes selon nouveau mapping :
  
  // SERVO 1 : Joystick 1 Horizontal (joy1H)
  if (abs(joy1H - joystickCenter) > JOYSTICK_DEADZONE) {
    int speed1x = map(abs(joy1H - joystickCenter), JOYSTICK_DEADZONE, 2048, 1, 5);
    
    if (joy1H < joystickCenter - JOYSTICK_DEADZONE) {
      servoAngles[0] = constrain(servoAngles[0] - speed1x, 0, 180);
    } else if (joy1H > joystickCenter + JOYSTICK_DEADZONE) {
      servoAngles[0] = constrain(servoAngles[0] + speed1x, 0, 180);
    }
  }
  
  // SERVO 2 : Joystick 1 Vertical (joy1V)
  if (abs(joy1V - joystickCenter) > JOYSTICK_DEADZONE) {
    int speed1y = map(abs(joy1V - joystickCenter), JOYSTICK_DEADZONE, 2048, 1, 5);
    
    if (joy1V < joystickCenter - JOYSTICK_DEADZONE) {
      servoAngles[1] = constrain(servoAngles[1] - speed1y, 0, 180);
    } else if (joy1V > joystickCenter + JOYSTICK_DEADZONE) {
      servoAngles[1] = constrain(servoAngles[1] + speed1y, 0, 180);
    }
  }
  
  // SERVO 3 : Joystick 2 Vertical (joy2V)
  if (abs(joy2V - joystickCenter) > JOYSTICK_DEADZONE) {
    int speed2y = map(abs(joy2V - joystickCenter), JOYSTICK_DEADZONE, 2048, 1, 5);
    
    if (joy2V < joystickCenter - JOYSTICK_DEADZONE) {
      servoAngles[2] = constrain(servoAngles[2] - speed2y, 0, 180);
    } else if (joy2V > joystickCenter + JOYSTICK_DEADZONE) {
      servoAngles[2] = constrain(servoAngles[2] + speed2y, 0, 180);
    }
  }
  
  // SERVO 4 : Joystick 2 Horizontal (joy2H)
  if (abs(joy2H - joystickCenter) > JOYSTICK_DEADZONE) {
    int speed2x = map(abs(joy2H - joystickCenter), JOYSTICK_DEADZONE, 2048, 1, 5);
    
    if (joy2H < joystickCenter - JOYSTICK_DEADZONE) {
      servoAngles[3] = constrain(servoAngles[3] - speed2x, 0, 180);
    } else if (joy2H > joystickCenter + JOYSTICK_DEADZONE) {
      servoAngles[3] = constrain(servoAngles[3] + speed2x, 0, 180);
    }
  }
  
  // SERVO 5 (Pince) : Bouton Joystick 2 - Toggle ouvert/fermé
  static bool lastJoy2Btn = HIGH;
  static unsigned long lastPinceToggle = 0;
  
  if (joy2Btn == LOW && lastJoy2Btn == HIGH && 
      (currentTime - lastPinceToggle > 500)) { // Debounce de 500ms
    
    // Toggle entre ouvert (0°) et fermé (180°)
    servoAngles[4] = (servoAngles[4] == 0) ? 180 : 0;
    lastPinceToggle = currentTime;
    
    Serial.print("Pince toggle: ");
    Serial.println(servoAngles[4] == 0 ? "OUVERTE" : "FERMÉE");
  }
  lastJoy2Btn = joy2Btn;
}

void RobotControl::sendCommand() {
  if (!espNowInitialized) return;
  
  // Préparer la commande
  currentCommand.servo1 = servoAngles[0];
  currentCommand.servo2 = servoAngles[1];  
  currentCommand.servo3 = servoAngles[2];
  currentCommand.servo4 = servoAngles[3];
  currentCommand.servo5 = servoAngles[4];
  currentCommand.mode = 0; // Mode manuel
  currentCommand.speed = 50; // Vitesse moyenne
  currentCommand.timestamp = millis();
  
  // Calculer le checksum
  currentCommand.checksum = 0;
  currentCommand.checksum = calculateChecksum(&currentCommand, sizeof(currentCommand));
  
  // Debug: afficher la commande
  Serial.print("ENVOI: [");
  Serial.print(currentCommand.servo1); Serial.print(",");
  Serial.print(currentCommand.servo2); Serial.print(",");
  Serial.print(currentCommand.servo3); Serial.print(",");
  Serial.print(currentCommand.servo4); Serial.print(",");
  Serial.print(currentCommand.servo5); Serial.print("] ");
  
  // Envoyer via ESP-NOW
  esp_err_t result = esp_now_send(robotArmMAC, (uint8_t*)&currentCommand, sizeof(currentCommand));
  
  if (result == ESP_OK) {
    lastCommandTime = millis();
    Serial.println("OK");
  } else {
    Serial.print("ERREUR: ");
    Serial.println(result);
  }
}

bool RobotControl::hasCommandChanged() {
  static RobotCommand lastCmd = {0};
  
  bool changed = (lastCmd.servo1 != currentCommand.servo1 ||
                  lastCmd.servo2 != currentCommand.servo2 ||
                  lastCmd.servo3 != currentCommand.servo3 ||
                  lastCmd.servo4 != currentCommand.servo4 ||
                  lastCmd.servo5 != currentCommand.servo5);
                  
  if (changed) {
    lastCmd = currentCommand;
  }
  
  return changed;
}

void RobotControl::draw() {
  display.clearDisplay();
  drawInterface();
  drawServoStatus();
  drawConnectionStatus();
  display.display();
}

void RobotControl::drawInterface() {
  // Titre
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("ROBOT CONTROL ");
  display.print((char)0x7E); // Flèche
  
  // Mode de contrôle (ligne 2)
  display.setCursor(0, 10);
  display.print("Mode: Simultane");
}

void RobotControl::drawServoStatus() {
  // Angles des servos (lignes 3-5)
  display.setCursor(0, 20);
  display.print("1:");
  display.print(servoAngles[0]);
  display.print("° 2:");
  display.print(servoAngles[1]);
  display.print("° 3:");
  display.print(servoAngles[2]);
  display.print("°");
  
  display.setCursor(0, 30);  
  display.print("4:");
  display.print(servoAngles[3]);
  display.print("° 5:");
  display.print(servoAngles[4]);
  display.print("°");
  
  // Affichage du mapping des contrôles
  display.setCursor(0, 40);
  display.print("Joy1:1&2 Joy2:3&4");
}

void RobotControl::drawConnectionStatus() {
  // État de connexion (ligne du bas)
  display.setCursor(0, 50);
  display.print("Conn: ");
  if (robotConnected) {
    display.print("OK");
    // Indicateur de signal
    int signalBars = 3; // Simulé pour l'instant
    for (int i = 0; i < signalBars; i++) {
      display.fillRect(40 + i * 3, 52 - i, 2, i + 2, SSD1306_WHITE);
    }
  } else {
    display.print("NONE");
  }
  
  // Instructions
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.print("BTN1:Menu J2BTN:Pince");
}

// Callbacks ESP-NOW
void RobotControl::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (instance) {
    instance->handleDataSent(mac_addr, status);
  }
}

void RobotControl::onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (instance) {
    instance->handleDataReceived(mac, incomingData, len);
  }
}

void RobotControl::handleDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Envoi réussi
  } else {
    Serial.println("Échec envoi ESP-NOW");
  }
}

void RobotControl::handleDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(RobotStatus)) {
    RobotStatus* status = (RobotStatus*)incomingData;
    
    if (validateStatus(*status)) {
      lastStatus = *status;
      lastStatusReceived = millis();
      robotConnected = true;
      
      Serial.print("Status reçu - Batterie: ");
      Serial.print(status->battery_level);
      Serial.println("%");
    }
  }
}

bool RobotControl::shouldReturnToMenu() {
  return requestReturnToMenu;
}

void RobotControl::reset() {
  // Remettre à la position neutre
  for (int i = 0; i < 5; i++) {
    servoAngles[i] = 90;
  }
  requestReturnToMenu = false;
}