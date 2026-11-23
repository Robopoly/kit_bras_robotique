#ifndef MENU_H
#define MENU_H

#include "../common/config.h"

class Menu {
private:
  int selectedIndex;
  int menuItems;
  int scrollOffset;  // Nouveau: pour gérer le défilement
  unsigned long lastInput;
  static const int INPUT_DELAY = 200; // ms entre les inputs
  static const int VISIBLE_ITEMS = 3; // Nombre d'options visibles simultanément
  
  // Debouncing pour bouton sélection
  bool lastSelectBtn;
  unsigned long lastButtonTime;
  static const unsigned long BUTTON_DEBOUNCE = 300; // 300ms debouncing
  bool gameSelected;
  
public:
  Menu();
  void init();
  void update();
  void draw();
  GameState getSelectedGame();
  void resetSelection();
  bool gameRequested();
};

#endif