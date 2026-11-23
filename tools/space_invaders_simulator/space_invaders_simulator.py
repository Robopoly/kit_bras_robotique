#!/usr/bin/env python3
"""
Simulateur Python du jeu Space Invaders pour ESP32
Permet de tester le code avant déploiement sur l'ESP

Contrôles:
- Flèche gauche: Déplacer à gauche
- Flèche droite: Déplacer à droite  
- Espace: Tirer
- R: Redémarrer le jeu
- ESC: Quitter

Auteur: Simulateur basé sur le code ESP32 C++
"""

import pygame
import sys
import time
import random

# Configuration de l'écran (simulation OLED 128x64)
SCREEN_WIDTH = 128
SCREEN_HEIGHT = 64
SCALE = 8  # Facteur d'agrandissement pour la visibilité
WINDOW_WIDTH = SCREEN_WIDTH * SCALE
WINDOW_HEIGHT = SCREEN_HEIGHT * SCALE

# Paramètres du jeu (identiques au code ESP32)
PLAYER_WIDTH = 8
PLAYER_HEIGHT = 4
INVADER_WIDTH = 6
INVADER_HEIGHT = 4
BULLET_WIDTH = 1
BULLET_HEIGHT = 3
MAX_BULLETS = 3
MAX_INVADER_BULLETS = 2
INVADERS_ROWS = 4
INVADERS_COLS = 8

# Couleurs (simulation OLED monochrome)
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GREEN = (0, 255, 0)  # Pour les éléments UI

class Player:
    def __init__(self):
        self.x = SCREEN_WIDTH // 2 - PLAYER_WIDTH // 2
        self.y = SCREEN_HEIGHT - PLAYER_HEIGHT - 2
        self.alive = True
        
    def update(self, keys):
        # Déplacement (identique au code ESP32)
        if keys[pygame.K_LEFT] and self.x > 0:
            self.x -= 2
        if keys[pygame.K_RIGHT] and self.x < SCREEN_WIDTH - PLAYER_WIDTH:
            self.x += 2
            
    def draw(self, screen):
        if self.alive:
            # Couleur cyan pour le vaisseau du joueur
            player_color = (0, 255, 255)  # Cyan
            
            # Dessiner un vaisseau stylisé au lieu d'un rectangle
            x = self.x * SCALE
            y = self.y * SCALE
            w = PLAYER_WIDTH * SCALE
            h = PLAYER_HEIGHT * SCALE
            
            # Corps principal du vaisseau (triangle)
            points = [
                (x + w//2, y),  # Pointe avant
                (x, y + h),     # Arrière gauche
                (x + w//4, y + h - SCALE),  # Encoche gauche
                (x + 3*w//4, y + h - SCALE),  # Encoche droite
                (x + w, y + h)  # Arrière droit
            ]
            pygame.draw.polygon(screen, player_color, points)
            
            # Détails du vaisseau (cockpit blanc)
            pygame.draw.circle(screen, WHITE, (x + w//2, y + h//2), SCALE//2)  # Cockpit
            
            # Réacteurs (effet lumineux)
            pygame.draw.circle(screen, (255, 255, 0), (x + w//4, y + h), SCALE//3)  # Réacteur gauche
            pygame.draw.circle(screen, (255, 255, 0), (x + 3*w//4, y + h), SCALE//3)  # Réacteur droit

class Bullet:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.active = False
        self.from_player = True
        
    def update(self):
        if self.active:
            if self.from_player:
                self.y -= 3  # Balles du joueur montrent
                if self.y < 0:
                    self.active = False
            else:
                self.y += 2  # Balles des envahisseurs descendent
                if self.y > SCREEN_HEIGHT:
                    self.active = False
                    
    def draw(self, screen):
        if self.active:
            x = self.x * SCALE
            y = self.y * SCALE
            w = BULLET_WIDTH * SCALE
            h = BULLET_HEIGHT * SCALE
            
            if self.from_player:
                # Balle du joueur (laser bleu)
                color = (0, 150, 255)  # Bleu laser
                pygame.draw.ellipse(screen, color, (x, y, w*2, h))
                pygame.draw.rect(screen, WHITE, (x + w//2, y, w//2, h))
            else:
                # Balle ennemie (rouge)
                color = (255, 50, 0)  # Rouge
                pygame.draw.ellipse(screen, color, (x, y, w*2, h))

class Invader:
    def __init__(self, x, y, row):
        self.x = x
        self.y = y
        self.alive = True
        self.row = row  # Pour déterminer la couleur
        
    def draw(self, screen):
        if self.alive:
            # Couleurs différentes selon la ligne
            colors = [
                (255, 100, 100),  # Rouge - ligne 0 (plus dangereuse)
                (255, 165, 0),    # Orange - ligne 1
                (255, 255, 0),    # Jaune - ligne 2
                (0, 255, 0)       # Vert - ligne 3 (moins dangereuse)
            ]
            color = colors[self.row % len(colors)]
            
            # Dessiner un envahisseur stylisé
            x = self.x * SCALE
            y = self.y * SCALE
            w = INVADER_WIDTH * SCALE
            h = INVADER_HEIGHT * SCALE
            
            # Corps de l'envahisseur (forme alien)
            # Tête
            pygame.draw.ellipse(screen, color, (x + SCALE, y, w - 2*SCALE, h//2))
            
            # Antennes
            pygame.draw.circle(screen, color, (x + SCALE, y), SCALE//2)
            pygame.draw.circle(screen, color, (x + w - SCALE, y), SCALE//2)
            
            # Corps
            pygame.draw.rect(screen, color, (x + SCALE//2, y + h//2, w - SCALE, h//2))
            
            # Pattes
            for i in range(3):
                px = x + i * (w//3) + SCALE//2
                pygame.draw.line(screen, color, (px, y + h), (px, y + h + SCALE//2), 2)
            
            # Yeux blancs pour plus de détail
            pygame.draw.circle(screen, WHITE, (x + w//3, y + h//4), SCALE//4)
            pygame.draw.circle(screen, WHITE, (x + 2*w//3, y + h//4), SCALE//4)

class SpaceInvadersGame:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
        pygame.display.set_caption("Space Invaders ESP32 Simulator")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 24)
        
        self.init_game()
        
    def init_game(self):
        """Initialisation du jeu (identique au code ESP32)"""
        self.player = Player()
        
        # Initialisation des balles
        self.bullets = []
        for i in range(MAX_BULLETS + MAX_INVADER_BULLETS):
            self.bullets.append(Bullet())
            
        # Initialisation des envahisseurs
        self.invaders = []
        for row in range(INVADERS_ROWS):
            invader_row = []
            for col in range(INVADERS_COLS):
                x = 10 + col * 12
                y = 8 + row * 8
                invader_row.append(Invader(x, y, row))  # Passer la ligne pour la couleur
            self.invaders.append(invader_row)
            
        self.invader_direction = 1
        self.last_invader_move = time.time()
        self.last_invader_shoot = time.time()
        self.score = 0
        self.lives = 3
        self.game_over = False
        self.game_won = False
        self.last_fire_time = 0
        
    def fire_bullet(self, x, y, from_player):
        """Créer une nouvelle balle"""
        start_index = 0 if from_player else MAX_BULLETS
        end_index = MAX_BULLETS if from_player else MAX_BULLETS + MAX_INVADER_BULLETS
        
        for i in range(start_index, end_index):
            if not self.bullets[i].active:
                self.bullets[i].x = x
                self.bullets[i].y = y
                self.bullets[i].active = True
                self.bullets[i].from_player = from_player
                break
                
    def invader_shoot(self):
        """Tir des envahisseurs"""
        for col in range(INVADERS_COLS):
            for row in range(INVADERS_ROWS - 1, -1, -1):
                if self.invaders[row][col].alive and random.randint(0, 19) == 0:
                    x = self.invaders[row][col].x + INVADER_WIDTH // 2
                    y = self.invaders[row][col].y + INVADER_HEIGHT
                    self.fire_bullet(x, y, False)
                    return  # Un seul tir à la fois
                    
    def update_invaders(self):
        """Mise à jour des envahisseurs (identique au code ESP32)"""
        current_time = time.time()
        
        # Déplacement des envahisseurs
        if current_time - self.last_invader_move > 0.5:
            change_direction = False
            
            # Vérifier si on doit changer de direction
            for row in range(INVADERS_ROWS):
                for col in range(INVADERS_COLS):
                    invader = self.invaders[row][col]
                    if invader.alive:
                        if ((self.invader_direction > 0 and invader.x >= SCREEN_WIDTH - INVADER_WIDTH) or
                            (self.invader_direction < 0 and invader.x <= 0)):
                            change_direction = True
                            break
                if change_direction:
                    break
                    
            if change_direction:
                self.invader_direction *= -1
                # Descendre les envahisseurs
                for row in range(INVADERS_ROWS):
                    for col in range(INVADERS_COLS):
                        invader = self.invaders[row][col]
                        if invader.alive:
                            invader.y += 4
                            # Game Over si les envahisseurs atteignent le joueur
                            if invader.y + INVADER_HEIGHT >= self.player.y:
                                self.game_over = True
            else:
                # Déplacer horizontalement
                for row in range(INVADERS_ROWS):
                    for col in range(INVADERS_COLS):
                        invader = self.invaders[row][col]
                        if invader.alive:
                            invader.x += self.invader_direction
                            
            self.last_invader_move = current_time
            
        # Tir des envahisseurs
        if current_time - self.last_invader_shoot > 1.0:
            self.invader_shoot()
            self.last_invader_shoot = current_time
            
    def check_collisions(self):
        """Vérifier les collisions"""
        for bullet in self.bullets:
            if not bullet.active:
                continue
                
            if bullet.from_player:
                # Collision balle joueur vs envahisseurs
                for row in range(INVADERS_ROWS):
                    for col in range(INVADERS_COLS):
                        invader = self.invaders[row][col]
                        if (invader.alive and
                            bullet.x >= invader.x and
                            bullet.x <= invader.x + INVADER_WIDTH and
                            bullet.y >= invader.y and
                            bullet.y <= invader.y + INVADER_HEIGHT):
                            invader.alive = False
                            bullet.active = False
                            self.score += 10
                            break
            else:
                # Collision balle envahisseur vs joueur
                if (bullet.x >= self.player.x and
                    bullet.x <= self.player.x + PLAYER_WIDTH and
                    bullet.y >= self.player.y and
                    bullet.y <= self.player.y + PLAYER_HEIGHT):
                    bullet.active = False
                    self.lives -= 1
                    if self.lives <= 0:
                        self.game_over = True
                        
    def check_game_won(self):
        """Vérifier si le jeu est gagné"""
        for row in range(INVADERS_ROWS):
            for col in range(INVADERS_COLS):
                if self.invaders[row][col].alive:
                    return False
        return True
        
    def draw_ui(self, screen):
        """Dessiner l'interface utilisateur"""
        # Score et vies (comme sur l'ESP32)
        score_text = self.font.render(f"Score: {self.score}", True, GREEN)
        lives_text = self.font.render(f"Lives: {self.lives}", True, GREEN)
        
        screen.blit(score_text, (10, 10))
        screen.blit(lives_text, (WINDOW_WIDTH - 120, 10))
        
        # Instructions en bas
        if not self.game_over and not self.game_won:
            controls_text = self.font.render("← → : Move, SPACE: Fire, R: Restart, ESC: Quit", True, GREEN)
            text_rect = controls_text.get_rect()
            text_rect.centerx = WINDOW_WIDTH // 2
            screen.blit(controls_text, (text_rect.x, WINDOW_HEIGHT - 30))
            
    def draw_game_over(self, screen):
        """Écran de fin de jeu"""
        if self.game_over:
            game_over_text = self.font.render("GAME OVER", True, WHITE)
            score_text = self.font.render(f"Score: {self.score}", True, WHITE)
        else:  # game_won
            game_over_text = self.font.render("YOU WIN!", True, WHITE)
            score_text = self.font.render(f"Score: {self.score}", True, WHITE)
            
        restart_text = self.font.render("Press R to restart", True, GREEN)
        
        # Centrer le texte
        game_over_rect = game_over_text.get_rect(center=(WINDOW_WIDTH//2, WINDOW_HEIGHT//2 - 40))
        score_rect = score_text.get_rect(center=(WINDOW_WIDTH//2, WINDOW_HEIGHT//2))
        restart_rect = restart_text.get_rect(center=(WINDOW_WIDTH//2, WINDOW_HEIGHT//2 + 40))
        
        screen.blit(game_over_text, game_over_rect)
        screen.blit(score_text, score_rect)
        screen.blit(restart_text, restart_rect)
        
    def run(self):
        """Boucle principale du jeu"""
        running = True
        
        while running:
            # Gestion des événements
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                    elif event.key == pygame.K_r:
                        self.init_game()
                    elif event.key == pygame.K_SPACE and not self.game_over and not self.game_won:
                        current_time = time.time()
                        if current_time - self.last_fire_time > 0.1:  # Limite de tir
                            self.fire_bullet(self.player.x + PLAYER_WIDTH // 2, self.player.y, True)
                            self.last_fire_time = current_time
            
            # Mise à jour du jeu
            if not self.game_over and not self.game_won:
                keys = pygame.key.get_pressed()
                self.player.update(keys)
                
                # Mise à jour des balles
                for bullet in self.bullets:
                    bullet.update()
                    
                self.update_invaders()
                self.check_collisions()
                
                if self.check_game_won():
                    self.game_won = True
            
            # Rendu
            self.screen.fill(BLACK)
            
            if self.game_over or self.game_won:
                self.draw_game_over(self.screen)
            else:
                # Dessiner le joueur
                self.player.draw(self.screen)
                
                # Dessiner les balles
                for bullet in self.bullets:
                    bullet.draw(self.screen)
                    
                # Dessiner les envahisseurs
                for row in range(INVADERS_ROWS):
                    for col in range(INVADERS_COLS):
                        self.invaders[row][col].draw(self.screen)
                        
            self.draw_ui(self.screen)
            
            pygame.display.flip()
            self.clock.tick(20)  # 20 FPS (similaire au delay(50) de l'ESP32)
            
        pygame.quit()

if __name__ == "__main__":
    try:
        game = SpaceInvadersGame()
        game.run()
    except KeyboardInterrupt:
        pygame.quit()
        sys.exit()