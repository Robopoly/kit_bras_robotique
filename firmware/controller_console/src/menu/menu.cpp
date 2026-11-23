#include "menu.h"

Menu::Menu() : selectedIndex(0), menuItems(7), scrollOffset(0), lastInput(0),
              lastSelectBtn(HIGH), lastButtonTime(0), gameSelected(false) {}

void Menu::init() {
  selectedIndex = 0;
  scrollOffset = 0;
  lastSelectBtn = HIGH;
  lastButtonTime = millis(); // Important: mettre le temps actuel pour éviter détection immédiate
  gameSelected = false;
}

void Menu::update() {
  unsigned long now = millis();
  if (now - lastInput < INPUT_DELAY) return;
  
  // Lecture joystick pour navigation
  int joystickY = analogRead(JOYSTICK_Y_PIN);
  int joystickAlt = analogRead(JOY1_V); // Test avec un autre pin
  bool selectBtn = digitalRead(JOYSTICK_BTN_PIN);
  
  // Debug: afficher les valeurs du joystick
  static unsigned long lastDebug = 0;
  if (now - lastDebug > 1000) { // Debug toutes les 1000 ms
    Serial.print("Menu Debug - JoystickY(GPIO2): ");
    Serial.print(joystickY);
    Serial.print(" JoystickAlt(GPIO1): ");
    Serial.print(joystickAlt);
    Serial.print(" Center: ");
    Serial.print(joystickCenter);
    Serial.print(" Btn: ");
    Serial.print(selectBtn == LOW ? "PRESSED" : "RELEASED");
    Serial.print(" Index: ");
    Serial.print(selectedIndex);

    // Debug batterie: lire et afficher la valeur ADC brute (VBAT via diviseur)
    int rawVbat = analogRead(VBAT_ADC_PIN);
    Serial.print(" VBAT raw: ");
    Serial.print(rawVbat);

    Serial.println();
    lastDebug = now;
  }
  
  // Navigation verticale dans le menu
  if (joystickY < (joystickCenter - JOYSTICK_DEADZONE)) {
    // Haut physique - sélection suivante (inversé)
    selectedIndex++;
    if (selectedIndex >= menuItems) selectedIndex = 0;
    
    // Ajuster le scroll pour garder l'élément sélectionné visible
    if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
      scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
    }
    if (selectedIndex == 0) { // Si on revient au début
      scrollOffset = 0;
    }
    
    lastInput = now;
    Serial.println("Menu: Navigation BAS (joystick haut)");
  }
  else if (joystickY > (joystickCenter + JOYSTICK_DEADZONE)) {
    // Bas physique - sélection précédente (inversé)
    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = menuItems - 1;
    
    // Ajuster le scroll pour garder l'élément sélectionné visible
    if (selectedIndex < scrollOffset) {
      scrollOffset = selectedIndex;
    }
    
    lastInput = now;
    Serial.println("Menu: Navigation HAUT (joystick bas)");
  }
  
  // Sélection avec bouton - avec debouncing
  if (selectBtn == LOW && lastSelectBtn == HIGH) {
    Serial.println("Menu: Bouton détecté!");
    if (now - lastButtonTime > BUTTON_DEBOUNCE) {
      Serial.print("Menu: Jeu sélectionné - Index: ");
      Serial.println(selectedIndex);
      gameSelected = true; // Marquer qu'un jeu a été sélectionné
      lastButtonTime = now;
    } else {
      Serial.println("Menu: Bouton ignoré (debounce)");
    }
  }
  lastSelectBtn = selectBtn;
}

void Menu::draw() {
  display.clearDisplay();
  
  // Titre compact
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 2);
  display.println("GAME MENU");
  
  // --- Batterie: lire ADC via diviseur (VBAT_ADC_PIN) et afficher pourcentage en haut à droite
  // Diviseur = 2 => Vbat = measured_voltage * 2
  int rawV = analogRead(VBAT_ADC_PIN);
  const float ADC_MAX = 2420.0f; // 12-bit ADC
  const float VREF = 2.05f;      // référence approximative
  float measuredV = (rawV / ADC_MAX) * VREF;
  float vbat = measuredV * 2.0f;
  // Map 3.0V -> 0% , 4.2V -> 100%
  int pct = (int)(((vbat - 3.0f) / (4.2f - 3.0f)) * 100.0f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  
  display.setCursor(100, 2);
  display.print(pct);
  display.print("%");
  
  // Noms des jeux
  const char* gameNames[] = {"1. PONG", "2. SPACE INVADERS", "3. PAC-MAN", "4. SNAKE", "5. ROBOT CONTROL", "6. DOOM", "7. DOOM (Ecran)"};
  
  // Afficher seulement les éléments visibles
  int startY = 15;
  int itemHeight = 16;
  
  for (int i = 0; i < VISIBLE_ITEMS && (scrollOffset + i) < menuItems; i++) {
    int itemIndex = scrollOffset + i;
    int itemY = startY + (i * itemHeight);
    
    // Fond de sélection
    if (itemIndex == selectedIndex) {
      display.fillRect(5, itemY - 2, 118, 14, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    // Texte de l'option
    display.setCursor(10, itemY);
    display.println(gameNames[itemIndex]);
  }
  
  // Indicateurs de scroll
  display.setTextColor(SSD1306_WHITE);
  if (scrollOffset > 0) {
    // Flèche haut
    display.setCursor(120, 15);
    display.println("^");
  }
  if (scrollOffset + VISIBLE_ITEMS < menuItems) {
    // Flèche bas
    display.setCursor(120, 40);
    display.println("v");
  }
  
  // Instructions compactes
  display.setTextSize(1);
  display.setCursor(5, 55);
  display.println("Joy:Select BTN:Play");
  
  display.display();
}

GameState Menu::getSelectedGame() {
  Serial.print("Menu::getSelectedGame() - selectedIndex = ");
  Serial.println(selectedIndex);
  
  switch (selectedIndex) {
    case 0: return PONG;
    case 1: return SPACE_INVADERS;
    case 2: return PACMAN;
    case 3: return SNAKE;
    case 4: return ROBOT_CONTROL;
    case 5: return DOOM;
    case 6: return DOOM_REMOTE;
    default: return MENU;
  }
}

void Menu::resetSelection() {
  selectedIndex = 0;
  gameSelected = false;
}

bool Menu::gameRequested() {
  return gameSelected;
}