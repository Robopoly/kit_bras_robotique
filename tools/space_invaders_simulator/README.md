# 🎮 Simulateur Space Invaders ESP32

Ce simulateur Python vous permet de tester votre jeu Space Invaders sur PC avant de le déployer sur votre ESP32 avec écran OLED.

## 🚀 Installation

### Prérequis
- Python 3.7 ou plus récent
- pip (gestionnaire de paquets Python)

### Installation des dépendances

```bash
# Dans le dossier tools/space_invaders_simulator
pip install -r requirements.txt
```

Ou installez pygame directement :
```bash
pip install pygame
```

## 🕹️ Utilisation

### Lancement du simulateur
```bash
python space_invaders_simulator.py
```

### Contrôles
- **←** (Flèche gauche) : Déplacer le vaisseau à gauche
- **→** (Flèche droite) : Déplacer le vaisseau à droite  
- **ESPACE** : Tirer
- **R** : Redémarrer la partie
- **ÉCHAP** : Quitter le jeu

## 🎯 Fonctionnalités

### Identique au code ESP32
- ✅ Déplacement du joueur avec les flèches
- ✅ Tir avec la barre d'espace (limite de 3 balles simultanées)
- ✅ Grille de 4x8 envahisseurs
- ✅ Mouvement en formation des envahisseurs
- ✅ Tir automatique des envahisseurs
- ✅ Système de collisions
- ✅ Score et vies
- ✅ Conditions de victoire/défaite
- ✅ Interface utilisateur

### Améliorations du simulateur
- 🖥️ Fenêtre agrandie pour une meilleure visibilité (simulation écran OLED 128x64 pixels)
- 🎨 Affichage en couleurs pour différencier les éléments
- ⌨️ Contrôles clavier intuitifs
- 🔄 Redémarrage facile avec la touche R

## 📊 Paramètres de jeu

```python
# Dimensions écran (identique ESP32)
SCREEN_WIDTH = 128
SCREEN_HEIGHT = 64

# Éléments de jeu
PLAYER_WIDTH = 8
PLAYER_HEIGHT = 4
INVADER_WIDTH = 6  
INVADER_HEIGHT = 4
MAX_BULLETS = 3
MAX_INVADER_BULLETS = 2
INVADERS_ROWS = 4
INVADERS_COLS = 8
```

## 🔄 Correspondance ESP32 ↔ PC

| ESP32 | PC | Description |
|-------|----| ----------- |
| `BTN_LEFT` (GPIO 5) | ← | Bouton gauche |
| `BTN_RIGHT` (GPIO 6) | → | Bouton droite |
| `BTN_FIRE` (GPIO 7) | ESPACE | Bouton tir |
| Écran OLED 128x64 | Fenêtre 1024x512 | Affichage (x8) |

## 🛠️ Développement

### Structure du code
```
space_invaders_simulator/
├── space_invaders_simulator.py  # Fichier principal
├── requirements.txt             # Dépendances Python
└── README.md                   # Ce fichier
```

### Classes principales
- `Player` : Gestion du vaisseau joueur
- `Bullet` : Gestion des projectiles
- `Invader` : Gestion des envahisseurs
- `SpaceInvadersGame` : Logique principale du jeu

## 🐛 Debugging

Le simulateur vous permet de :
- Tester la logique de jeu sans matériel
- Vérifier les collisions et mouvements
- Ajuster les paramètres facilement
- Identifier les bugs avant déploiement ESP32

## 📝 Notes de portage

### Différences avec l'ESP32
1. **Timing** : Le simulateur utilise `time.time()` au lieu de `millis()`
2. **Affichage** : Pygame au lieu de l'écran OLED SSD1306
3. **Entrées** : Clavier au lieu des boutons GPIO
4. **Couleurs** : Support couleur pour meilleure UX

### Fidélité au code original
- ✅ Même logique de mouvement des envahisseurs
- ✅ Même système de tir et collisions  
- ✅ Mêmes conditions de victoire/défaite
- ✅ Mêmes paramètres de jeu

## 🚀 Prochaines étapes

1. Testez votre logique de jeu dans le simulateur
2. Ajustez les paramètres si nécessaire
3. Transférez les modifications vers le code ESP32
4. Déployez sur votre ESP32 avec confiance !

---
**Bon jeu ! 🎮**