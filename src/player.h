#pragma once
#include "raylib.h"

enum class Stance {
    MAJOR,
    MINOR
};

static const int PLAYER_MAX_HEALTH = 100;

struct Player {
    Vector2 position;
    Vector2 velocity;
    float width;
    float height;
    bool onGround;
    Stance stance;
    bool facingRight;
    bool isAttacking;
    float attackTimer;
    float flashTimer;
    int health;
    float dashTimer;
    bool justJumped;
    bool attackHasHit;
    float baseWidth;      // width without the beer bloat effect
    float beerBlinkTimer; // green blink feedback right after drinking
    float beerSlowTimer;  // bloated + slowed for the rest of this duration
};

Player PlayerCreate(Vector2 startPos);
void PlayerUpdate(Player &player, float dt, float groundY, float screenWidth);
void PlayerDraw(const Player &player, Texture2D texture);
const char *StanceToString(Stance stance);
// Starts a quick dash burst in the player's current facing direction.
void PlayerStartDash(Player &player);
// The melee swing's hitbox this frame; zero-area rect when not attacking.
Rectangle PlayerGetAttackHitbox(const Player &player);
// Heals 20 HP, blinks green 3 times, and bloats the player (2x width,
// 75% slower movement) for 5 seconds.
void PlayerDrinkBeer(Player &player);
