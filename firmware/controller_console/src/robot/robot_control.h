#ifndef ROBOT_CONTROL_H
#define ROBOT_CONTROL_H

#include "../common/config.h"
#include "robot_comm.h"
#include <esp_now.h>
#include <WiFi.h>

class RobotControl {
private:
  // État de la communication
  bool espNowInitialized;
  bool robotConnected;
  unsigned long lastCommandTime;
  unsigned long lastStatusReceived;
  
  // Commande actuelle
  RobotCommand currentCommand;
  RobotStatus lastStatus;
  
  // Interface utilisateur
  int servoAngles[5];      // Angles actuels des servos (0-180°)
  int joystickCenter;      // Centre des joysticks
  bool calibrated;         // Calibration joysticks
  
  // Variables de contrôle
  unsigned long lastMenuButtonTime;
  bool lastMenuBtn;
  bool requestReturnToMenu;
  unsigned long lastTransmission;
  
  // Constantes
  static const unsigned long COMMAND_TIMEOUT = 1000;    // 1 seconde
  static const unsigned long TRANSMISSION_RATE = 50;    // 50ms = 20Hz
  static const int ANGLE_STEP = 5;                      // Pas d'angle pour contrôle fin
  
  // Fonctions privées
  void initESPNow();
  void sendCommand();
  void updateFromJoysticks();
  void drawInterface();
  void drawServoStatus();
  void drawConnectionStatus();
  bool hasCommandChanged();
  
  // Callbacks ESP-NOW
  static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
  static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
  
public:
  RobotControl();
  void init();
  void update();
  void draw();
  bool shouldReturnToMenu();
  void reset();
  
  // Fonctions statiques pour callbacks
  static RobotControl* instance;
  void handleDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
  void handleDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
};

#endif