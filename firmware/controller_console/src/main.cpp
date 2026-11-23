/*
 * Système de menu principal pour ESP32
 * Navigation entre Pong, Space Invaders, Pac-Man, Snake et Doom
 */

#include "common/config.h"
#include "menu/menu.h"
#include "games/pong_game.h"
#include "games/space_invaders_game.h"
#include "games/pacman_game.h"
#include "games/snake_game.h"
#include "games/doom_game.h"
#include "games/doom_ctrl.h"
#include "robot/robot_control.h"

// Instances des jeux et menu
Menu mainMenu;
PongGame pongGame;
SpaceInvadersGame spaceInvadersGame;
PacManGame pacmanGame;
SnakeGame snakeGame;
RobotControl robotControl;
DoomGame*   doomGame   = nullptr; // Pointeur car nécessite display initialisé
DoomCtrl*   doomCtrl   = nullptr; // Controller mode: streams inputs to Waveshare

void setup() {
  Serial.begin(115200);
  
  Serial.println("=== GAME SYSTEM STARTED ===");
  Serial.println("Menu Principal: Joystick Y: GPIO2, BTN: GPIO6");
  Serial.println("Pong: JOY1_V: GPIO1, JOY2_V: GPIO3, BTN1: GPIO5, BTN2: GPIO6");
  Serial.println("Space Invaders: Joystick Y: GPIO2, Tir: GPIO6, Menu: GPIO5");
  
  // Initialisation du système
  initDisplay();
  initJoystick();
  
  // Initialiser DoomGame APRÈS l'initialisation du display
  doomGame = new DoomGame(&display);
  doomCtrl = new DoomCtrl(&display);
  
  // Calibration du joystick
  delay(100);
  joystickCenter = analogRead(JOYSTICK_Y_PIN);
  delay(100);
  Serial.print("Joystick calibré au centre: ");
  Serial.println(joystickCenter);
  
  // Initialisation du menu
  mainMenu.init();
  currentState = MENU;
  
  Serial.println("Système prêt!");
}

void loop() {
  switch (currentState) {
    case MENU:
      mainMenu.update();
      mainMenu.draw();
      
      // Vérifier si un jeu a été demandé
      if (mainMenu.gameRequested()) {
        GameState requestedGame = mainMenu.getSelectedGame();
        Serial.print("Main: Jeu demandé - Index enum: ");
        Serial.println(requestedGame);
        
        if (requestedGame == PONG) {
          currentState = PONG;
          pongGame.init();
          Serial.println("Démarrage de Pong");
        } else if (requestedGame == SPACE_INVADERS) {
          Serial.println("Main: Tentative de lancement de Space Invaders");
          currentState = SPACE_INVADERS;
          spaceInvadersGame.init();
          Serial.println("Main: Space Invaders initialisé, état changé");
          Serial.print("Main: État actuel = ");
          Serial.println(currentState == SPACE_INVADERS ? "SPACE_INVADERS" : "AUTRE");
        } else if (requestedGame == PACMAN) {
          Serial.println("Main: Tentative de lancement de Pac-Man");
          currentState = PACMAN;
          pacmanGame.init();
          Serial.println("Main: Pac-Man initialisé, état changé");
        } else if (requestedGame == SNAKE) {
          Serial.println("Main: Tentative de lancement de Snake");
          currentState = SNAKE;
          snakeGame.init();
          Serial.println("Main: Snake initialisé, état changé");
        } else if (requestedGame == ROBOT_CONTROL) {
          Serial.println("Main: Tentative de lancement du Robot Control");
          currentState = ROBOT_CONTROL;
          robotControl.init();
          Serial.println("Main: Robot Control initialisé, état changé");
        } else if (requestedGame == DOOM) {
          Serial.println("Main: Tentative de lancement de Doom");
          currentState = DOOM;
          doomGame->init();
          Serial.println("Main: Doom initialisé, état changé");
        } else if (requestedGame == DOOM_REMOTE) {
          Serial.println("Main: Lancement DOOM Controller (Waveshare)");
          currentState = DOOM_REMOTE;
          doomCtrl->init();
        }
        mainMenu.resetSelection(); // Reset après sélection
      }
      break;
      
    case PONG:
      pongGame.update();
      pongGame.draw();
      
      // Vérifier retour au menu
      if (pongGame.shouldReturnToMenu()) {
        currentState = MENU;
        mainMenu.init();
        pongGame.init(); // Réinitialiser le jeu pour éviter les états résiduels
        Serial.println("Retour au menu principal");
      }
      break;
      
    case SPACE_INVADERS:
      // Debug: confirmer qu'on est dans Space Invaders
      static unsigned long lastSpaceDebug = 0;
      if (millis() - lastSpaceDebug > 2000) {
        Serial.println("Main: Dans la boucle Space Invaders");
        lastSpaceDebug = millis();
      }
      
      spaceInvadersGame.update();
      spaceInvadersGame.draw();
      
      // Vérifier retour au menu
      if (spaceInvadersGame.shouldReturnToMenu()) {
        currentState = MENU;
        mainMenu.init();
        spaceInvadersGame.init(); // Réinitialiser le jeu pour éviter les états résiduels
        Serial.println("Retour au menu principal");
      }
      break;
      
    case PACMAN:
      pacmanGame.update();
      pacmanGame.draw();
      
      // Vérifier retour au menu
      if (pacmanGame.shouldReturnToMenu()) {
        currentState = MENU;
        mainMenu.init();
        pacmanGame.init(); // Réinitialiser le jeu pour éviter les états résiduels
        Serial.println("Retour au menu principal depuis Pac-Man");
      }
      break;
      
    case SNAKE:
      snakeGame.update();
      snakeGame.draw();
      
      // Vérifier retour au menu
      if (snakeGame.shouldReturnToMenu()) {
        currentState = MENU;
        mainMenu.init();
        snakeGame.init(); // Réinitialiser le jeu pour éviter les états résiduels
        Serial.println("Retour au menu principal depuis Snake");
      }
      break;
      
    case ROBOT_CONTROL:
      robotControl.update();
      robotControl.draw();
      
      // Vérifier retour au menu
      if (robotControl.shouldReturnToMenu()) {
        currentState = MENU;
        mainMenu.init();
        robotControl.reset(); // Réinitialiser le contrôle robot
        Serial.println("Retour au menu principal depuis Robot Control");
      }
      break;
      
    case DOOM:
      {
        int joy1V = analogRead(JOY1_V)     - 2048;
        int joy1H = analogRead(JOY1_H_PIN) - 2048;
        int joy2V = analogRead(JOY2_V)     - 2048;
        int joy2H = analogRead(JOY2_H)     - 2048;
        bool btnA = digitalRead(JOY1_BTN_PIN) == LOW;
        bool btnB = digitalRead(JOY2_BTN)     == LOW;

        doomGame->update(joy1V, joy1H, joy2V, joy2H, btnA, btnB);
        doomGame->draw();

        if (doomGame->shouldReturnToMenu()) {
          currentState = MENU;
          mainMenu.init();
          doomGame->init();
          Serial.println("Retour au menu depuis Doom local");
        }
      }
      break;

    case DOOM_REMOTE:
      {
        /* Read joysticks -- already centred to [-2048..+2048] */
        int joy1V = analogRead(JOY1_V)     - 2048;
        int joy1H = analogRead(JOY1_H_PIN) - 2048;
        int joy2V = analogRead(JOY2_V)     - 2048;
        int joy2H = analogRead(JOY2_H)     - 2048;
        bool btnA = digitalRead(JOY1_BTN_PIN) == LOW;
        bool btnB = digitalRead(JOY2_BTN)     == LOW;

        doomCtrl->update(joy1V, joy1H, joy2V, joy2H, btnA, btnB);
        doomCtrl->draw();

        if (doomCtrl->shouldReturnToMenu()) {
          currentState = MENU;
          mainMenu.init();
          Serial.println("Retour au menu depuis DOOM Controller");
        }
      }
      break;
  }
  
  delay(50); // FPS control
}
