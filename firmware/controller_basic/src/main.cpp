/*
 * Manette Kit JIG - Version "Bras Robotique" (sans écran)
 * Contrôle uniquement le bras robotique (projet Robot_Arm_ESP32) via ESP-NOW.
 * Aucun jeu, aucun écran OLED requis : les infos passent par le moniteur série.
 */

#include <esp_now.h>
#include <WiFi.h>

// ========== CONFIGURATION ==========

// Broches joysticks (identiques à Kit_JIG_controller_test)
#define JOY1_V_PIN   1  // Joystick 1 vertical   -> Servo 2 (Épaule)
#define JOY1_H_PIN   2  // Joystick 1 horizontal -> Servo 1 (Base)
#define JOY2_V_PIN   3  // Joystick 2 vertical   -> Servo 3 (Coude)
#define JOY2_H_PIN   4  // Joystick 2 horizontal -> Servo 4 (Poignet)
#define JOY1_BTN_PIN 5  // Bouton joystick 1     -> Position de repos
#define JOY2_BTN_PIN 6  // Bouton joystick 2     -> Pince (ouvrir/fermer)

// Moteurs de vibration du PCB (maintenus éteints)
#define HAPTIC1_PIN 17
#define HAPTIC2_PIN 18

#define JOYSTICK_DEADZONE 300

// Temporisation
const unsigned long CONTROL_RATE = 50;         // Lecture joysticks + envoi : 20 Hz
const unsigned long KEEPALIVE_RATE = 200;      // Renvoi même sans mouvement (le bras se met en sécurité après 2 s sans commande)
const unsigned long CONNECTION_TIMEOUT = 1000; // Bras considéré déconnecté sans statut reçu
const unsigned long BUTTON_DEBOUNCE = 500;
const unsigned long STATUS_PRINT_RATE = 1000;

// ============================================================================
// /!\ MAC DU BRAS À REMPLACER PAR CELLE DE VOTRE PROPRE BRAS /!\
// La valeur ci-dessous est celle du bras du créateur : avec elle, votre bras
// ne bougera PAS.
//
// Pour la trouver : flasher le bras (projet Robot_Arm_ESP32), ouvrir le moniteur
// série à 115200 bauds et lire la ligne "MAC du bras: XX:XX:XX:XX:XX:XX".
// Exemple : FC:01:2C:08:FC:9C -> {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}
//
// La MAC de CETTE manette s'affiche au démarrage ("MAC de la manette: ...") :
// elle doit être recopiée dans controllerMAC de Robot_Arm_ESP32/src/main.cpp.
// Voir le README pour la procédure complète.
// ============================================================================
uint8_t robotArmMAC[6] = {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}; // -> À REMPLACER

// ========== STRUCTURES DE DONNÉES ==========
// /!\ Doivent rester IDENTIQUES à celles de Robot_Arm_ESP32/src/main.cpp

struct RobotCommand {
  uint8_t servo1;
  uint8_t servo2;
  uint8_t servo3;
  uint8_t servo4;
  uint8_t servo5;
  uint8_t mode;
  uint8_t speed;
  uint32_t timestamp;
  uint32_t checksum;
};

struct RobotStatus {
  uint8_t battery_level;
  uint8_t connection_rssi;
  uint8_t servo_positions[5];
  bool emergency_stop;
  uint32_t timestamp;
  uint32_t checksum;
};

// ========== VARIABLES GLOBALES ==========

// Angles envoyés au bras : Base, Épaule, Coude, Poignet, Pince
int servoAngles[5] = {90, 90, 90, 90, 90};
int lastSentAngles[5] = {-1, -1, -1, -1, -1};

// Axe joystick qui pilote chaque servo 1 à 4, et son centre mesuré au démarrage
const int axisPins[4] = {JOY1_H_PIN, JOY1_V_PIN, JOY2_V_PIN, JOY2_H_PIN};
int axisCenter[4] = {2048, 2048, 2048, 2048};

RobotCommand currentCommand;
RobotStatus lastStatus;
volatile unsigned long lastStatusReceived = 0; // Écrit dans le callback ESP-NOW

bool espNowInitialized = false;
bool robotConnected = false;

// Contrôle
unsigned long lastControl = 0;
unsigned long lastSent = 0;
unsigned long lastStatusPrint = 0;
bool lastJoy1Btn = HIGH;
bool lastJoy2Btn = HIGH;
unsigned long lastJoy1BtnTime = 0;
unsigned long lastJoy2BtnTime = 0;

// ========== FONCTIONS UTILITAIRES ==========

uint32_t calculateChecksum(const void* data, size_t size) {
  uint32_t checksum = 0;
  const uint8_t* bytes = (const uint8_t*)data;

  for (size_t i = 0; i < size - sizeof(uint32_t); i++) {
    checksum += bytes[i];
    checksum = (checksum << 1) | (checksum >> 31);
  }

  return checksum;
}

bool validateStatus(const RobotStatus& status) {
  if (status.battery_level > 100) {
    return false;
  }

  for (int i = 0; i < 5; i++) {
    if (status.servo_positions[i] > 180) {
      return false;
    }
  }

  RobotStatus tempStatus = status;
  tempStatus.checksum = 0;
  uint32_t expectedChecksum = calculateChecksum(&tempStatus, sizeof(tempStatus));

  return (status.checksum == expectedChecksum);
}

void printMAC(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    Serial.printf("%02X", mac[i]);
  }
  Serial.println();
}

// ========== GESTION ESP-NOW ==========

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(RobotStatus)) return;

  RobotStatus status;
  memcpy(&status, incomingData, sizeof(status));

  if (validateStatus(status)) {
    lastStatus = status;
    lastStatusReceived = millis();
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optionnel
}

void initESPNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("MAC de la manette: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERREUR: ESP-NOW init");
    return;
  }

  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);

  // Ajouter le bras comme peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, robotArmMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.print("ERREUR: Ajout peer bras ");
    printMAC(robotArmMAC);
    return;
  }

  espNowInitialized = true;
  Serial.print("ESP-NOW initialisé - Peer bras: ");
  printMAC(robotArmMAC);
}

void sendCommand() {
  if (!espNowInitialized) return;

  currentCommand.servo1 = servoAngles[0];
  currentCommand.servo2 = servoAngles[1];
  currentCommand.servo3 = servoAngles[2];
  currentCommand.servo4 = servoAngles[3];
  currentCommand.servo5 = servoAngles[4];
  currentCommand.mode = 0;    // Mode manuel
  currentCommand.speed = 50;  // Vitesse moyenne
  currentCommand.timestamp = millis();

  currentCommand.checksum = 0;
  currentCommand.checksum = calculateChecksum(&currentCommand, sizeof(currentCommand));

  esp_err_t result = esp_now_send(robotArmMAC, (uint8_t*)&currentCommand, sizeof(currentCommand));

  if (result != ESP_OK) {
    Serial.print("Erreur envoi commande: ");
    Serial.println(result);
  }
}

// ========== JOYSTICKS ==========

void initInputs() {
  pinMode(JOY1_BTN_PIN, INPUT_PULLUP);
  pinMode(JOY2_BTN_PIN, INPUT_PULLUP);

  // Moteurs de vibration éteints dès le boot
  pinMode(HAPTIC1_PIN, OUTPUT);
  pinMode(HAPTIC2_PIN, OUTPUT);
  digitalWrite(HAPTIC1_PIN, LOW);
  digitalWrite(HAPTIC2_PIN, LOW);
}

// Mesure le centre de chaque axe : ne pas toucher les joysticks au démarrage
void calibrateJoysticks() {
  for (int a = 0; a < 4; a++) {
    long sum = 0;
    for (int s = 0; s < 16; s++) {
      sum += analogRead(axisPins[a]);
      delay(2);
    }
    axisCenter[a] = sum / 16;
  }

  Serial.print("Calibration joysticks (centres): ");
  for (int a = 0; a < 4; a++) {
    Serial.print(axisCenter[a]);
    if (a < 3) Serial.print(",");
  }
  Serial.println();
}

void updateFromJoysticks(unsigned long now) {
  // Servos 1 à 4 : plus le joystick est poussé, plus le servo tourne vite
  for (int a = 0; a < 4; a++) {
    int offset = analogRead(axisPins[a]) - axisCenter[a];

    if (abs(offset) > JOYSTICK_DEADZONE) {
      int speed = constrain(map(abs(offset), JOYSTICK_DEADZONE, 2048, 1, 5), 1, 5);
      servoAngles[a] = constrain(servoAngles[a] + (offset > 0 ? speed : -speed), 0, 180);
    }
  }

  // Bouton joystick 2 : pince ouverte (0°) / fermée (180°)
  bool joy2Btn = digitalRead(JOY2_BTN_PIN);
  if (joy2Btn == LOW && lastJoy2Btn == HIGH && now - lastJoy2BtnTime > BUTTON_DEBOUNCE) {
    servoAngles[4] = (servoAngles[4] == 0) ? 180 : 0;
    lastJoy2BtnTime = now;
    Serial.print("Pince: ");
    Serial.println(servoAngles[4] == 0 ? "OUVERTE" : "FERMÉE");
  }
  lastJoy2Btn = joy2Btn;

  // Bouton joystick 1 : servos 1 à 4 en position de repos (la pince ne bouge pas)
  bool joy1Btn = digitalRead(JOY1_BTN_PIN);
  if (joy1Btn == LOW && lastJoy1Btn == HIGH && now - lastJoy1BtnTime > BUTTON_DEBOUNCE) {
    for (int i = 0; i < 4; i++) {
      servoAngles[i] = 90;
    }
    lastJoy1BtnTime = now;
    Serial.println("Position de repos");
  }
  lastJoy1Btn = joy1Btn;
}

bool anglesChanged() {
  for (int i = 0; i < 5; i++) {
    if (servoAngles[i] != lastSentAngles[i]) return true;
  }
  return false;
}

// ========== SETUP ET LOOP ==========

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("===== MANETTE KIT JIG - BRAS ROBOTIQUE =====");

  initInputs();
  calibrateJoysticks();
  initESPNow();

  Serial.println("Joy1 H/V: Base/Épaule | Joy2 V/H: Coude/Poignet");
  Serial.println("Bouton Joy2: Pince    | Bouton Joy1: Position de repos");
  Serial.println("=============================================");
}

void loop() {
  // Lire lastStatusReceived AVANT millis() : le callback peut l'écrire à tout moment
  unsigned long lastStatusTime = lastStatusReceived;
  unsigned long now = millis();

  // Lecture joysticks et envoi au bras
  if (now - lastControl >= CONTROL_RATE) {
    updateFromJoysticks(now);

    if (anglesChanged() || now - lastSent >= KEEPALIVE_RATE) {
      sendCommand();
      memcpy(lastSentAngles, servoAngles, sizeof(servoAngles));
      lastSent = now;
    }
    lastControl = now;
  }

  // État de la connexion (le bras renvoie son statut toutes les 100 ms)
  bool connected = lastStatusTime != 0 && now - lastStatusTime < CONNECTION_TIMEOUT;
  if (connected != robotConnected) {
    robotConnected = connected;
    Serial.println(connected ? ">>> Bras CONNECTÉ" : ">>> Bras DÉCONNECTÉ (vérifier MAC / alimentation)");
  }

  // Debug périodique
  if (now - lastStatusPrint >= STATUS_PRINT_RATE) {
    Serial.print("Angles: [");
    for (int i = 0; i < 5; i++) {
      Serial.print(servoAngles[i]);
      if (i < 4) Serial.print(",");
    }
    Serial.print("] | Bras: ");
    Serial.println(robotConnected ? "OK" : "NONE");
    lastStatusPrint = now;
  }

  delay(5);
}
