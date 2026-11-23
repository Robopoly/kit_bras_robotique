#include "robot_comm.h"

// ============================================================================
// /!\ ADRESSES MAC À REMPLACER PAR CELLES DE VOS PROPRES CARTES /!\
// Les valeurs ci-dessous sont celles des cartes du créateur : avec elles,
// le mode ROBOT CONTROL ne trouvera PAS votre bras.
//
// robotArmMAC : MAC du bras robotique (projet firmware/arm).
//   Pour la trouver : flasher le bras, ouvrir le moniteur série à 115200 bauds
//   et lire la ligne "MAC du bras: XX:XX:XX:XX:XX:XX" affichée au démarrage.
//   Exemple : FC:01:2C:08:FC:9C -> {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}
//
// controllerMAC : MAC de cette manette. Non utilisée par la manette elle-même,
//   elle doit être recopiée dans controllerMAC de firmware/arm/src/main.cpp.
//   Voir le README pour la procédure complète.
// ============================================================================
uint8_t robotArmMAC[6] = {0xFC, 0x01, 0x2C, 0x08, 0xFC, 0x9C}; // MAC du bras  -> À REMPLACER
uint8_t controllerMAC[6] = {0x98, 0x88, 0xe0, 0xa7, 0x6f, 0x9c}; // MAC manette -> À REMPLACER

// Adresse broadcast pour découverte Pong multijoueur
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t calculateChecksum(const void* data, size_t size) {
  uint32_t checksum = 0;
  const uint8_t* bytes = (const uint8_t*)data;
  
  for (size_t i = 0; i < size - sizeof(uint32_t); i++) {
    checksum += bytes[i];
    checksum = (checksum << 1) | (checksum >> 31); // Rotation
  }
  
  return checksum;
}

bool validateCommand(const RobotCommand& cmd) {
  // Vérifier les limites des angles des servos (0-180°)
  if (cmd.servo1 > 180 || cmd.servo2 > 180 || cmd.servo3 > 180 || 
      cmd.servo4 > 180 || cmd.servo5 > 180) {
    return false;
  }
  
  // Vérifier le mode (0-255 mais on peut limiter)
  if (cmd.mode > 10) {
    return false;
  }
  
  // Vérifier la vitesse (0-100%)
  if (cmd.speed > 100) {
    return false;
  }
  
  // Vérifier le checksum
  RobotCommand tempCmd = cmd;
  tempCmd.checksum = 0; // Reset pour calcul
  uint32_t expectedChecksum = calculateChecksum(&tempCmd, sizeof(tempCmd));
  
  return (cmd.checksum == expectedChecksum);
}

bool validateStatus(const RobotStatus& status) {
  // Vérifier le niveau de batterie (0-100%)
  if (status.battery_level > 100) {
    return false;
  }
  
  // Vérifier les positions des servos (0-180°)
  for (int i = 0; i < 5; i++) {
    if (status.servo_positions[i] > 180) {
      return false;
    }
  }
  
  // Vérifier le checksum
  RobotStatus tempStatus = status;
  tempStatus.checksum = 0; // Reset pour calcul
  uint32_t expectedChecksum = calculateChecksum(&tempStatus, sizeof(tempStatus));
  
  return (status.checksum == expectedChecksum);
}