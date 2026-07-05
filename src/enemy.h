#pragma once
#include "raylib.h"

static const int ENEMY_MAX_HEALTH = 100;
static const int GOBLIN_ATTACK_DAMAGE = 10;
static const int MINOR_ENEMY_ATTACK_DAMAGE = 5; // bat / spider

enum class EnemyType {
    GOBLIN,
    BAT,
    SPIDER,
    SLIME
};

struct Enemy {
    EnemyType type;
    Vector2 position;
    float width;
    float height;
    int health;
    bool facingRight;
    float staggerTimer;
    float velocityX;
    float velocityY; // only used by flying types (bat)
    float wanderTimer;
    float animTimer;
    float burnTimer;
    float burnDamagePerSecond;
    float burnAccumulator;
    float attackCooldownTimer;
    float attackAnimTimer;
};

// Picks a random enemy type and spawns it at a random location.
Enemy EnemyCreate(float groundY, float screenWidth);
// Non-goblin types wander at random within the screen bounds (the bat also
// drifts and bobs within a band above the ground). Goblins instead chase the
// player's horizontal position. Also ticks down any active burn and the
// attack cooldown/animation. Returns true if the burn just killed (and
// respawned) the enemy this frame.
bool EnemyUpdate(Enemy &enemy, float dt, float groundY, float screenWidth, float playerCenterX);
// Applies damage; respawns as a new random type at a random location once
// health hits 0. Returns true if this hit killed (and respawned) the enemy.
bool EnemyTakeDamage(Enemy &enemy, int damage, float groundY, float screenWidth);
// Starts (or refreshes) a damage-over-time burn lasting `duration` seconds,
// dealing `totalDamage` spread evenly across that time.
void EnemyIgnite(Enemy &enemy, int totalDamage, float duration);
// Pushes the enemy away from `fromCenter` by `force`, clamped to the screen.
void EnemyKnockback(Enemy &enemy, Vector2 fromCenter, float force, float screenWidth);
// Goblins, bats, and spiders all attack (slimes don't), only once per second
// per enemy, and only within melee range of the player. Returns true on a
// hit (and starts its attack animation); the caller applies
// EnemyAttackDamage(enemy.type) to the player.
bool EnemyTryAttackPlayer(Enemy &enemy, Rectangle playerRect);
void EnemyDraw(const Enemy &enemy);
// Souls awarded for killing this type: goblin 30, slime 15, bat 10, spider 5.
int EnemySoulValue(EnemyType type);
// Damage dealt on a successful EnemyTryAttackPlayer hit: goblin 10, bat/spider 5.
int EnemyAttackDamage(EnemyType type);
