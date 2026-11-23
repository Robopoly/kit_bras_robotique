#!/usr/bin/env python3
"""
Script de démonstration du simulateur Space Invaders ESP32

Ce script lance le simulateur et affiche des informations de debug
pour aider au développement et au test.
"""

import pygame
import sys
import os

def main():
    print("=" * 60)
    print("🎮 SIMULATEUR SPACE INVADERS ESP32")
    print("=" * 60)
    print()
    print("📋 Informations système:")
    print(f"   - Python: {sys.version.split()[0]}")
    print(f"   - Pygame: {pygame.version.ver}")
    print(f"   - Répertoire: {os.getcwd()}")
    print()
    print("🕹️  Contrôles:")
    print("   ← →  : Déplacer le vaisseau")
    print("   ESPACE : Tirer")
    print("   R    : Redémarrer")
    print("   ÉCHAP : Quitter")
    print()
    print("🎯 Correspondance ESP32 ↔ PC:")
    print("   BTN_LEFT (GPIO 5)  → ← (Flèche gauche)")
    print("   BTN_RIGHT (GPIO 6) → → (Flèche droite)")
    print("   BTN_FIRE (GPIO 7)  → ESPACE")
    print()
    print("🔧 Paramètres de jeu (identiques ESP32):")
    print("   - Écran: 128x64 pixels (agrandi 8x pour PC)")
    print("   - Envahisseurs: 4 lignes × 8 colonnes") 
    print("   - Balles max: 3 joueur + 2 envahisseurs")
    print("   - Vies: 3")
    print()
    print("▶️  Lancement du simulateur...")
    print("   (Fermez la fenêtre de jeu ou appuyez ÉCHAP pour revenir)")
    print()
    
    # Import et lancement du jeu
    try:
        from space_invaders_simulator import SpaceInvadersGame
        
        game = SpaceInvadersGame()
        game.run()
        
        print()
        print("✅ Simulateur fermé normalement")
        
    except ImportError:
        print("❌ Erreur: Impossible d'importer le simulateur")
        print("   Vérifiez que space_invaders_simulator.py est présent")
        return 1
        
    except Exception as e:
        print(f"❌ Erreur lors de l'exécution: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())