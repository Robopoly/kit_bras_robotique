/*
 * ESP32 Bras Robotique - Version corrigée
 * Fix pour l'erreur "Peer interface is invalid"
 */

#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// ========== CONFIGURATION ==========

// Pins des servomoteurs (GPIO 1 à 5)
#define SERVO1_PIN 1  // Base
#define SERVO2_PIN 2  // Épaule
#define SERVO3_PIN 3  // Coude  
#define SERVO4_PIN 4  // Poignet
#define SERVO5_PIN 5  // Pince

// Pin LED d'état
#define STATUS_LED_PIN 8

// ========== STRUCTURES DE DONNÉES ==========

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

// Objets servomoteurs
Servo servos[5];

// État actuel
int currentPositions[5] = {90, 90, 90, 90, 90};
int targetPositions[5] = {90, 90, 90, 90, 90};
RobotCommand lastCommand;
RobotStatus currentStatus;

// Contrôle
unsigned long lastCommandReceived = 0;
unsigned long lastMovementUpdate = 0;
unsigned long lastStatusSent = 0;
unsigned long lastDebug = 0;
bool emergencyStop = false;
bool systemInitialized = false;
unsigned long receivedCount = 0;

// Temporisation
const unsigned long COMMAND_TIMEOUT = 2000;
const unsigned long MOVEMENT_RATE = 20;
const unsigned long STATUS_RATE = 100;
const int MAX_MOVEMENT_SPEED = 3;

// ============================================================================
// /!\ MAC DE LA MANETTE À REMPLACER PAR CELLE DE VOTRE PROPRE MANETTE /!\
// La valeur ci-dessous est celle de la manette du créateur : avec elle, le bras
// n'enverra pas son statut à votre manette ("Conn: NONE" / "Bras: NONE").
//
// Pour la trouver : flasher la manette, ouvrir le moniteur série à 115200 bauds :
//  - avec écran (firmware/controller_console) : lancer "ROBOT CONTROL" dans le menu
//    et lire la ligne "MAC Address: XX:XX:XX:XX:XX:XX" ;
//  - sans écran (firmware/controller_basic) : appuyer sur RESET et lire
//    la ligne "MAC de la manette: XX:XX:XX:XX:XX:XX".
// Exemple : 98:88:E0:A7:6F:9C -> {0x98, 0x88, 0xE0, 0xA7, 0x6F, 0x9C}
// Voir le README pour la procédure complète.
// ============================================================================
uint8_t controllerMAC[6] = {0x98, 0x88, 0xe0, 0xa7, 0x6f, 0x9c}; // -> À REMPLACER

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

bool validateCommand(const RobotCommand& cmd) {
  if (cmd.servo1 > 180 || cmd.servo2 > 180 || cmd.servo3 > 180 || 
      cmd.servo4 > 180 || cmd.servo5 > 180) {
    return false;
  }
  
  if (cmd.mode > 10 || cmd.speed > 100) {
    return false;
  }
  
  RobotCommand tempCmd = cmd;
  tempCmd.checksum = 0;
  uint32_t expectedChecksum = calculateChecksum(&tempCmd, sizeof(tempCmd));
  
  return (cmd.checksum == expectedChecksum);
}

void setEmergencyStop(bool stop) {
  emergencyStop = stop;
  digitalWrite(STATUS_LED_PIN, stop ? HIGH : LOW);
  
  if (stop) {
    Serial.println("ARRÊT D'URGENCE ACTIVÉ");
  } else {
    Serial.println("Arrêt d'urgence désactivé");
  }
}

void moveToSafePosition() {
  for (int i = 0; i < 5; i++) {
    targetPositions[i] = 90;
  }
  Serial.println("Passage en position de sécurité");
}

// ========== GESTION ESP-NOW ==========

void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
  receivedCount++;
  
  if (len != sizeof(RobotCommand)) {
    Serial.print("ERREUR: Taille incorrecte - reçu ");
    Serial.print(len);
    Serial.print(" attendu ");
    Serial.println(sizeof(RobotCommand));
    return;
  }
  
  RobotCommand* cmd = (RobotCommand*)incomingData;
  
  if (!validateCommand(*cmd)) {
    Serial.println("ERREUR: Commande invalide");
    return;
  }
  
  if (cmd->timestamp == lastCommand.timestamp) {
    return; // Commande déjà traitée
  }
  
  // Traiter la commande
  lastCommand = *cmd;
  lastCommandReceived = millis();
  
  // Appliquer les nouvelles positions
  targetPositions[0] = constrain(cmd->servo1, 0, 180);
  targetPositions[1] = constrain(cmd->servo2, 0, 180);
  targetPositions[2] = constrain(cmd->servo3, 0, 180);
  targetPositions[3] = constrain(cmd->servo4, 0, 180);
  targetPositions[4] = constrain(cmd->servo5, 0, 180);
  
  if (emergencyStop) {
    setEmergencyStop(false);
  }
  
  Serial.print("REÇU #");
  Serial.print(receivedCount);
  Serial.print(": [");
  Serial.print(cmd->servo1); Serial.print(",");
  Serial.print(cmd->servo2); Serial.print(",");
  Serial.print(cmd->servo3); Serial.print(",");
  Serial.print(cmd->servo4); Serial.print(",");
  Serial.print(cmd->servo5); Serial.println("]");
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optionnel
}

void initESPNow() {
  // Configurer WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  Serial.print("MAC du bras: ");
  Serial.println(WiFi.macAddress());
  
  // Initialiser ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERREUR: ESP-NOW init");
    return;
  }
  
  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);
  
  // Ajouter la manette comme peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, controllerMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA; // Interface WiFi Station
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ERREUR: Ajout peer manette");
    Serial.print("MAC peer: ");
    for(int i = 0; i < 6; i++) {
      if(i > 0) Serial.print(":");
      Serial.printf("%02X", controllerMAC[i]);
    }
    Serial.println();
    return;
  }
  
  Serial.println("ESP-NOW initialisé avec succès");
  Serial.print("Peer manette: ");
  for(int i = 0; i < 6; i++) {
    if(i > 0) Serial.print(":");
    Serial.printf("%02X", controllerMAC[i]);
  }
  Serial.println();
}

void sendStatus() {
  currentStatus.battery_level = 85;
  currentStatus.connection_rssi = 75;
  currentStatus.emergency_stop = emergencyStop;
  currentStatus.timestamp = millis();
  
  for (int i = 0; i < 5; i++) {
    currentStatus.servo_positions[i] = currentPositions[i];
  }
  
  currentStatus.checksum = 0;
  currentStatus.checksum = calculateChecksum(&currentStatus, sizeof(currentStatus));
  
  esp_err_t result = esp_now_send(controllerMAC, (uint8_t*)&currentStatus, sizeof(currentStatus));
  
  if (result != ESP_OK) {
    Serial.print("Erreur envoi status: ");
    Serial.println(result);
  }
}

// ========== CONTRÔLE SERVOMOTEURS ==========

void initServos() {
  Serial.println("Initialisation des servomoteurs...");
  
  for (int i = 0; i < 5; i++) {
    int pin = i + 1;
    servos[i].attach(pin);
    servos[i].write(90);
    currentPositions[i] = 90;
    targetPositions[i] = 90;
    
    Serial.print("Servo ");
    Serial.print(i + 1);
    Serial.print(" → GPIO");
    Serial.println(pin);
    
    delay(100);
  }
  
  Serial.println("Servomoteurs prêts");
}

void updateServoPositions() {
  // Pas de return en arrêt d'urgence : le bras doit pouvoir rejoindre
  // lentement la position de sécurité fixée par moveToSafePosition()
  bool hasMovement = false;
  
  for (int i = 0; i < 5; i++) {
    if (currentPositions[i] != targetPositions[i]) {
      hasMovement = true;
      
      int difference = targetPositions[i] - currentPositions[i];
      int step = (difference > 0) ? min(MAX_MOVEMENT_SPEED, difference) : 
                                   max(-MAX_MOVEMENT_SPEED, difference);
      
      currentPositions[i] += step;
      servos[i].write(currentPositions[i]);
    }
  }
  
  if (hasMovement) {
    Serial.print("Positions: ");
    for (int i = 0; i < 5; i++) {
      Serial.print(currentPositions[i]);
      if (i < 4) Serial.print(",");
    }
    Serial.println();
  }
}

// ========== SETUP ET LOOP ==========

void setup() {
  Serial.begin(115200);
  Serial.println("===== ESP32 BRAS ROBOTIQUE v2 =====");
  
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  initServos();
  initESPNow();
  moveToSafePosition();
  
  systemInitialized = true;
  Serial.println("Système prêt - En attente commandes...");
  Serial.println("=====================================");
}

void loop() {
  if (!systemInitialized) return;
  
  unsigned long currentTime = millis();
  
  // Vérifier timeout
  if (currentTime - lastCommandReceived > COMMAND_TIMEOUT && !emergencyStop) {
    setEmergencyStop(true);
    moveToSafePosition();
  }
  
  // Mettre à jour servos
  if (currentTime - lastMovementUpdate > MOVEMENT_RATE) {
    updateServoPositions();
    lastMovementUpdate = currentTime;
  }
  
  // Envoyer status
  if (currentTime - lastStatusSent > STATUS_RATE) {
    sendStatus();
    lastStatusSent = currentTime;
  }
  
  // Debug périodique
  if (currentTime - lastDebug > 5000) {
    Serial.print("Status: ");
    Serial.print(emergencyStop ? "STOP" : "OK");
    Serial.print(" | Reçu: ");
    Serial.print(receivedCount);
    Serial.print(" | Dernière commande: ");
    Serial.print((currentTime - lastCommandReceived) / 1000);
    Serial.println("s");
    lastDebug = currentTime;
  }
  
  delay(10);
}