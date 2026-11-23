#include "config.h"

// Variables globales
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
int joystickCenter = 2048;
GameState currentState = MENU;
bool requestReturnToMenu = false;

void initDisplay() {
  Wire.begin(8, 9); // SDA=8, SCL=9 pour ESP32-S3
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Échec allocation SSD1306"));
    for (;;);
  }
  
  // Rotation de l'écran (2 = 180 degrés)
  display.setRotation(2);
  display.clearDisplay();
  display.display();
}

void initJoystick() {
  pinMode(JOYSTICK_BTN_PIN, INPUT_PULLUP);
  pinMode(JOY1_BTN, INPUT_PULLUP);
  pinMode(JOY2_BTN, INPUT_PULLUP);

  // Haptics: garantir moteurs éteints dès le boot
  pinMode(17, OUTPUT);
  pinMode(18, OUTPUT);
  digitalWrite(17, LOW);
  digitalWrite(18, LOW);
}