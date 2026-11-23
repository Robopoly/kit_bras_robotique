#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// === Configuration écran OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
extern Adafruit_SSD1306 display;

// === Broches joystick ===
#define JOYSTICK_Y_PIN 1   // GPIO1 (JOY1_V) - axe Y pour mouvement menu
#define JOYSTICK_X_PIN 3   // GPIO3 (JOY2_V) - axe X pour mouvement horizontal
#define JOY1_H_PIN 2       // GPIO2 (JOY1_H) - axe horizontal du joystick 1
#define JOYSTICK_BTN_PIN 6 // GPIO6 (JOY2_BTN) - bouton du 2e joystick
#define JOYSTICK_DEADZONE 300

// === Configuration joystick Pong ===
#define JOY1_V 1   // GPIO1
#define JOY2_V 3   // GPIO3
#define JOY2_H 4   // GPIO4 - Joystick 2 Horizontal
#define JOY1_BTN 5 // GPIO5
#define JOY2_BTN 6 // GPIO6
#define JOY1_BTN_PIN 5 // GPIO5 - alias pour compatibilité Snake

// === Batterie ADC ===
// Le diviseur de tension divise par 2, VBAT_ADC_PIN lit la moitié de la tension
#define VBAT_ADC_PIN 10 // GPIO10 - entrée ADC pour mesurer la tension batterie via diviseur

// Variables pour calibration joystick
extern int joystickCenter;

// États du système
enum GameState {
  MENU,
  PONG,
  SPACE_INVADERS,
  PACMAN,
  SNAKE,
  ROBOT_CONTROL,
  DOOM,
  DOOM_REMOTE   /* controller mode: streams inputs to Waveshare via ESP-NOW */
};

extern GameState currentState;
extern bool requestReturnToMenu;

// Fonctions communes
void initDisplay();
void initJoystick();

#endif