#include "enemy.h"
#include <cmath>

static const float HEALTH_BAR_WIDTH = 20.0f;
static const float HEALTH_BAR_HEIGHT = 3.0f;
static const float STAGGER_DURATION = 0.2f;

static const float WANDER_INTERVAL_MIN = 1.0f;
static const float WANDER_INTERVAL_MAX = 3.0f;

static const float GOBLIN_WIDTH = 14.0f,  GOBLIN_HEIGHT = 18.0f,  GOBLIN_SPEED = 18.0f;
static const float BAT_WIDTH = 14.0f,     BAT_HEIGHT = 10.0f,     BAT_SPEED = 30.0f;
static const float SPIDER_WIDTH = 16.0f,  SPIDER_HEIGHT = 10.0f,  SPIDER_SPEED = 35.0f;
static const float SLIME_WIDTH = 14.0f,   SLIME_HEIGHT = 10.0f,   SLIME_SPEED = 10.0f;

static const float GOBLIN_ATTACK_RANGE = 4.0f; // gap between goblin and player hitboxes that still counts as "in range"
static const float GOBLIN_ATTACK_COOLDOWN = 1.0f;
static const float GOBLIN_ATTACK_ANIM_DURATION = 0.25f;

static float RandomWanderInterval() {
    return GetRandomValue((int)(WANDER_INTERVAL_MIN * 1000), (int)(WANDER_INTERVAL_MAX * 1000)) / 1000.0f;
}

static float SpeedForType(EnemyType type) {
    switch (type) {
        case EnemyType::BAT:    return BAT_SPEED;
        case EnemyType::SPIDER: return SPIDER_SPEED;
        case EnemyType::SLIME:  return SLIME_SPEED;
        default:                return GOBLIN_SPEED;
    }
}

// Resets an enemy to a freshly-chosen random type at a random spawn point.
static void RandomizeEnemy(Enemy &enemy, float groundY, float screenWidth) {
    enemy.type = (EnemyType)GetRandomValue(0, 3);
    switch (enemy.type) {
        case EnemyType::BAT:    enemy.width = BAT_WIDTH;    enemy.height = BAT_HEIGHT;    break;
        case EnemyType::SPIDER: enemy.width = SPIDER_WIDTH; enemy.height = SPIDER_HEIGHT; break;
        case EnemyType::SLIME:  enemy.width = SLIME_WIDTH;  enemy.height = SLIME_HEIGHT;  break;
        default:                enemy.width = GOBLIN_WIDTH; enemy.height = GOBLIN_HEIGHT; break;
    }

    float x = (float)GetRandomValue(0, (int)(screenWidth - enemy.width));
    if (enemy.type == EnemyType::BAT) {
        enemy.position = {x, groundY - enemy.height - (float)GetRandomValue(20, 70)};
    } else {
        enemy.position = {x, groundY - enemy.height};
    }

    enemy.health = ENEMY_MAX_HEALTH;
    enemy.facingRight = true;
    enemy.staggerTimer = 0.0f;
    enemy.velocityX = 0.0f;
    enemy.velocityY = 0.0f;
    enemy.wanderTimer = RandomWanderInterval();
    enemy.animTimer = 0.0f;
    enemy.burnTimer = 0.0f;
    enemy.burnDamagePerSecond = 0.0f;
    enemy.burnAccumulator = 0.0f;
    enemy.attackCooldownTimer = 0.0f;
    enemy.attackAnimTimer = 0.0f;
}

Enemy EnemyCreate(float groundY, float screenWidth) {
    Enemy enemy;
    RandomizeEnemy(enemy, groundY, screenWidth);
    return enemy;
}

void EnemyIgnite(Enemy &enemy, int totalDamage, float duration) {
    enemy.burnTimer = duration;
    enemy.burnDamagePerSecond = (duration > 0.0f) ? (totalDamage / duration) : 0.0f;
    enemy.burnAccumulator = 0.0f;
}

bool EnemyUpdate(Enemy &enemy, float dt, float groundY, float screenWidth, float playerCenterX) {
    enemy.animTimer += dt;

    if (enemy.attackCooldownTimer > 0.0f) {
        enemy.attackCooldownTimer -= dt;
        if (enemy.attackCooldownTimer < 0.0f) enemy.attackCooldownTimer = 0.0f;
    }
    if (enemy.attackAnimTimer > 0.0f) {
        enemy.attackAnimTimer -= dt;
        if (enemy.attackAnimTimer < 0.0f) enemy.attackAnimTimer = 0.0f;
    }

    if (enemy.burnTimer > 0.0f) {
        enemy.burnTimer -= dt;
        if (enemy.burnTimer < 0.0f) enemy.burnTimer = 0.0f;

        enemy.burnAccumulator += enemy.burnDamagePerSecond * dt;
        int wholeDamage = (int)enemy.burnAccumulator;
        if (wholeDamage > 0) {
            enemy.burnAccumulator -= wholeDamage;
            enemy.health -= wholeDamage;
            if (enemy.health <= 0) {
                RandomizeEnemy(enemy, groundY, screenWidth);
                return true;
            }
        }
    }

    if (enemy.staggerTimer > 0.0f) {
        enemy.staggerTimer -= dt;
        if (enemy.staggerTimer < 0.0f) enemy.staggerTimer = 0.0f;
        return false; // stunned; skip wandering this frame
    }

    float speed = SpeedForType(enemy.type);
    bool chasesPlayer = (enemy.type == EnemyType::GOBLIN || enemy.type == EnemyType::BAT || enemy.type == EnemyType::SPIDER);

    if (chasesPlayer) {
        // Goblins, bats, and spiders all chase the player's horizontal
        // position instead of wandering randomly. Only slimes wander.
        float enemyCenterX = enemy.position.x + enemy.width / 2.0f;
        if (playerCenterX < enemyCenterX - 1.0f) enemy.velocityX = -speed;
        else if (playerCenterX > enemyCenterX + 1.0f) enemy.velocityX = speed;
        else enemy.velocityX = 0.0f;

        if (enemy.velocityX < 0.0f) enemy.facingRight = false;
        else if (enemy.velocityX > 0.0f) enemy.facingRight = true;
    } else {
        enemy.wanderTimer -= dt;
        if (enemy.wanderTimer <= 0.0f) {
            enemy.velocityX = GetRandomValue(-1, 1) * speed;
            enemy.wanderTimer = RandomWanderInterval();
        }
    }

    // Bats keep an independent vertical bob/drift regardless of whether
    // they're chasing horizontally.
    if (enemy.type == EnemyType::BAT) {
        enemy.wanderTimer -= dt;
        if (enemy.wanderTimer <= 0.0f) {
            enemy.velocityY = GetRandomValue(-1, 1) * speed * 0.6f;
            enemy.wanderTimer = RandomWanderInterval();
        }
    }

    enemy.position.x += enemy.velocityX * dt;
    if (enemy.position.x < 0.0f) {
        enemy.position.x = 0.0f;
        enemy.velocityX = fabsf(enemy.velocityX);
    }
    if (enemy.position.x + enemy.width > screenWidth) {
        enemy.position.x = screenWidth - enemy.width;
        enemy.velocityX = -fabsf(enemy.velocityX);
    }

    if (enemy.type == EnemyType::BAT) {
        enemy.position.y += enemy.velocityY * dt + sinf(enemy.animTimer * 4.0f) * 6.0f * dt;
        // Kept low enough that a jumping melee swing can still reach it --
        // the player's jump apex is only ~32px, so anything higher is unfair.
        float minY = groundY - enemy.height - 40.0f;
        float maxY = groundY - enemy.height - 10.0f;
        if (enemy.position.y < minY) {
            enemy.position.y = minY;
            enemy.velocityY = fabsf(enemy.velocityY);
        }
        if (enemy.position.y > maxY) {
            enemy.position.y = maxY;
            enemy.velocityY = -fabsf(enemy.velocityY);
        }
    }
    return false;
}

bool EnemyTakeDamage(Enemy &enemy, int damage, float groundY, float screenWidth) {
    enemy.health -= damage;
    if (enemy.health <= 0) {
        RandomizeEnemy(enemy, groundY, screenWidth);
        return true;
    }
    enemy.staggerTimer = STAGGER_DURATION;
    return false;
}

void EnemyKnockback(Enemy &enemy, Vector2 fromCenter, float force, float screenWidth) {
    float enemyCenterX = enemy.position.x + enemy.width / 2.0f;
    float direction = (enemyCenterX < fromCenter.x) ? -1.0f : 1.0f;
    enemy.position.x += direction * force;
    if (enemy.position.x < 0.0f) enemy.position.x = 0.0f;
    if (enemy.position.x + enemy.width > screenWidth) enemy.position.x = screenWidth - enemy.width;
}

bool EnemyTryAttackPlayer(Enemy &enemy, Rectangle playerRect) {
    if (enemy.type == EnemyType::SLIME) return false;
    if (enemy.attackCooldownTimer > 0.0f) return false;

    Rectangle enemyRect = {enemy.position.x, enemy.position.y, enemy.width, enemy.height};
    Rectangle attackZone = {
        enemyRect.x - GOBLIN_ATTACK_RANGE, enemyRect.y,
        enemyRect.width + 2.0f * GOBLIN_ATTACK_RANGE, enemyRect.height
    };
    if (CheckCollisionRecs(attackZone, playerRect)) {
        enemy.attackCooldownTimer = GOBLIN_ATTACK_COOLDOWN;
        enemy.attackAnimTimer = GOBLIN_ATTACK_ANIM_DURATION;
        return true;
    }
    return false;
}

int EnemySoulValue(EnemyType type) {
    switch (type) {
        case EnemyType::BAT:    return 10;
        case EnemyType::SPIDER: return 5;
        case EnemyType::SLIME:  return 15;
        default:                return 30; // goblin
    }
}

int EnemyAttackDamage(EnemyType type) {
    switch (type) {
        case EnemyType::BAT:
        case EnemyType::SPIDER: return MINOR_ENEMY_ATTACK_DAMAGE;
        case EnemyType::SLIME:  return 0; // slimes never attack
        default:                return GOBLIN_ATTACK_DAMAGE;
    }
}

// A small goblin: round green body and head, pointy ears, a held pouch.
// While attackAnimTimer is running it lunges toward the player and flashes
// a claw-swipe -- otherwise a goblin just standing next to the player while
// damage ticks looks like nothing is happening.
static void DrawGoblin(const Enemy &enemy, float shakeX, Color skinColor, Color skinShadow) {
    float facingDir = enemy.facingRight ? 1.0f : -1.0f;
    float lunge = 0.0f;
    if (enemy.attackAnimTimer > 0.0f) {
        float lungeProgress = 1.0f - (enemy.attackAnimTimer / GOBLIN_ATTACK_ANIM_DURATION);
        lunge = sinf(lungeProgress * PI) * 3.0f * facingDir;
    }

    float cx = enemy.position.x + shakeX + lunge + enemy.width / 2.0f;
    float headCenterY = enemy.position.y + enemy.height * 0.28f;
    float headRadius = enemy.width * 0.32f;
    float bodyCenterY = enemy.position.y + enemy.height * 0.68f;
    float bodyRadiusH = enemy.width * 0.4f;
    float bodyRadiusV = enemy.height * 0.32f;

    DrawRectangle((int)(cx - bodyRadiusH * 0.6f), (int)(enemy.position.y + enemy.height * 0.85f), 3, (int)(enemy.height * 0.15f), skinShadow);
    DrawRectangle((int)(cx + bodyRadiusH * 0.6f - 3), (int)(enemy.position.y + enemy.height * 0.85f), 3, (int)(enemy.height * 0.15f), skinShadow);

    DrawEllipse((int)cx, (int)bodyCenterY, bodyRadiusH, bodyRadiusV, skinColor);
    DrawEllipseLines((int)cx, (int)bodyCenterY, bodyRadiusH, bodyRadiusV, BLACK);

    DrawRectangle((int)(cx - 3), (int)(bodyCenterY - 1), 6, 5, BROWN);
    DrawRectangleLines((int)(cx - 3), (int)(bodyCenterY - 1), 6, 5, BLACK);

    Vector2 leftEarBase1 = {cx - headRadius * 0.8f, headCenterY - headRadius * 0.3f};
    Vector2 leftEarBase2 = {cx - headRadius * 0.2f, headCenterY - headRadius * 0.6f};
    Vector2 leftEarTip = {cx - headRadius * 1.8f, headCenterY - headRadius * 0.9f};
    DrawTriangle(leftEarBase2, leftEarBase1, leftEarTip, skinColor);
    DrawTriangleLines(leftEarBase2, leftEarBase1, leftEarTip, BLACK);

    Vector2 rightEarBase1 = {cx + headRadius * 0.8f, headCenterY - headRadius * 0.3f};
    Vector2 rightEarBase2 = {cx + headRadius * 0.2f, headCenterY - headRadius * 0.6f};
    Vector2 rightEarTip = {cx + headRadius * 1.8f, headCenterY - headRadius * 0.9f};
    DrawTriangle(rightEarBase1, rightEarBase2, rightEarTip, skinColor);
    DrawTriangleLines(rightEarBase1, rightEarBase2, rightEarTip, BLACK);

    DrawCircle((int)cx, (int)headCenterY, headRadius, skinColor);
    DrawCircleLines((int)cx, (int)headCenterY, headRadius, BLACK);

    DrawCircle((int)(cx - headRadius * 0.4f), (int)headCenterY, 1.2f, BLACK);
    DrawCircle((int)(cx + headRadius * 0.4f), (int)headCenterY, 1.2f, BLACK);

    if (enemy.attackAnimTimer > 0.0f) {
        float fade = enemy.attackAnimTimer / GOBLIN_ATTACK_ANIM_DURATION;
        float slashX = cx + facingDir * (bodyRadiusH + 3.0f);
        float slashY = bodyCenterY;
        Color slashColor = Fade(WHITE, fade);
        DrawLineEx({slashX - facingDir * 3, slashY - 3}, {slashX + facingDir * 3, slashY + 3}, 1.5f, slashColor);
        DrawLineEx({slashX - facingDir * 3, slashY}, {slashX + facingDir * 3, slashY}, 1.5f, slashColor);
        DrawLineEx({slashX - facingDir * 3, slashY + 3}, {slashX + facingDir * 3, slashY - 3}, 1.5f, slashColor);
    }
}

// A small flying bat: flapping wings, round body, pointed ears.
static void DrawBat(const Enemy &enemy, float shakeX, Color bodyColor) {
    float cx = enemy.position.x + shakeX + enemy.width / 2.0f;
    float cy = enemy.position.y + enemy.height / 2.0f;
    float wingFlap = sinf(enemy.animTimer * 12.0f) * 0.5f + 0.5f; // 0..1
    float wingSpread = enemy.width * 0.6f * (0.5f + wingFlap * 0.5f);

    DrawTriangle({cx, cy}, {cx - wingSpread, cy - 4 - wingFlap * 3}, {cx - wingSpread * 0.5f, cy + 3}, bodyColor);
    DrawTriangle({cx, cy}, {cx + wingSpread * 0.5f, cy + 3}, {cx + wingSpread, cy - 4 - wingFlap * 3}, bodyColor);

    DrawCircle((int)cx, (int)cy, enemy.width * 0.28f, bodyColor);

    DrawTriangle({cx - 3, cy - enemy.height * 0.3f}, {cx - 1, cy - enemy.height * 0.55f}, {cx + 1, cy - enemy.height * 0.3f}, bodyColor);
    DrawTriangle({cx - 1, cy - enemy.height * 0.3f}, {cx + 1, cy - enemy.height * 0.55f}, {cx + 3, cy - enemy.height * 0.3f}, bodyColor);

    DrawCircle((int)(cx - 2), (int)cy, 1.0f, RED);
    DrawCircle((int)(cx + 2), (int)cy, 1.0f, RED);
}

// A small spider: round abdomen and head, skittering legs.
static void DrawSpider(const Enemy &enemy, float shakeX, Color bodyColor) {
    float cx = enemy.position.x + shakeX + enemy.width / 2.0f;
    float cy = enemy.position.y + enemy.height * 0.6f;
    float legWag = sinf(enemy.animTimer * 10.0f) * 2.0f;

    for (int i = 0; i < 4; i++) {
        float legY = cy - 3 + i * 2;
        float wag = (i % 2 == 0) ? legWag : -legWag;
        DrawLineEx({cx - enemy.width * 0.15f, legY}, {cx - enemy.width * 0.5f, legY - 3 + wag}, 1.0f, BLACK);
        DrawLineEx({cx + enemy.width * 0.15f, legY}, {cx + enemy.width * 0.5f, legY - 3 + wag}, 1.0f, BLACK);
    }

    DrawCircle((int)cx, (int)cy, enemy.width * 0.3f, bodyColor);
    DrawCircle((int)cx, (int)(cy - enemy.height * 0.3f), enemy.width * 0.18f, bodyColor);

    DrawCircle((int)(cx - 1.5f), (int)(cy - enemy.height * 0.3f), 0.8f, RED);
    DrawCircle((int)(cx + 1.5f), (int)(cy - enemy.height * 0.3f), 0.8f, RED);
}

// A small slime: a squishing, bouncing blob.
static void DrawSlime(const Enemy &enemy, float shakeX, Color bodyColor) {
    float squish = (sinf(enemy.animTimer * 5.0f) * 0.5f + 0.5f) * 0.25f; // 0..0.25
    float w = enemy.width * (1.0f + squish);
    float h = enemy.height * (1.0f - squish);
    float cx = enemy.position.x + shakeX + enemy.width / 2.0f;
    float baseY = enemy.position.y + enemy.height;
    float cy = baseY - h / 2.0f;

    DrawEllipse((int)cx, (int)cy, w / 2.0f, h / 2.0f, bodyColor);
    DrawEllipseLines((int)cx, (int)cy, w / 2.0f, h / 2.0f, BLACK);

    DrawCircle((int)(cx - w * 0.15f), (int)(cy - h * 0.1f), 1.0f, BLACK);
    DrawCircle((int)(cx + w * 0.15f), (int)(cy - h * 0.1f), 1.0f, BLACK);
    DrawCircle((int)(cx - w * 0.2f), (int)(cy - h * 0.25f), w * 0.12f, Fade(WHITE, 0.4f));
}

void EnemyDraw(const Enemy &enemy) {
    bool staggered = enemy.staggerTimer > 0.0f;
    float shakeX = staggered ? sinf(enemy.staggerTimer * 60.0f) * 2.0f : 0.0f;

    switch (enemy.type) {
        case EnemyType::BAT:
            DrawBat(enemy, shakeX, staggered ? RED : Color{60, 55, 75, 255});
            break;
        case EnemyType::SPIDER:
            DrawSpider(enemy, shakeX, staggered ? RED : Color{40, 35, 45, 255});
            break;
        case EnemyType::SLIME:
            DrawSlime(enemy, shakeX, staggered ? Color{220, 80, 80, 200} : Color{90, 200, 140, 200});
            break;
        default:
            DrawGoblin(enemy, shakeX, staggered ? RED : Color{94, 153, 66, 255}, staggered ? MAROON : Color{60, 110, 45, 255});
            break;
    }

    if (enemy.burnTimer > 0.0f) {
        float flicker = sinf(enemy.animTimer * 20.0f) * 1.5f;
        Vector2 flamePos = {enemy.position.x + enemy.width * 0.5f + flicker, enemy.position.y + enemy.height * 0.2f};
        DrawCircleV(flamePos, 2.0f, Fade(ORANGE, 0.8f));
        DrawCircleV({flamePos.x - 3, flamePos.y + 2}, 1.3f, Fade(RED, 0.7f));
        DrawCircleV({flamePos.x + 3, flamePos.y + 2}, 1.3f, Fade(RED, 0.7f));
    }

    float barX = enemy.position.x + enemy.width / 2 - HEALTH_BAR_WIDTH / 2;
    float barY = enemy.position.y - HEALTH_BAR_HEIGHT - 3;
    float healthRatio = (float)enemy.health / ENEMY_MAX_HEALTH;
    DrawRectangle((int)barX, (int)barY, (int)HEALTH_BAR_WIDTH, (int)HEALTH_BAR_HEIGHT, DARKGRAY);
    DrawRectangle((int)barX, (int)barY, (int)(HEALTH_BAR_WIDTH * healthRatio), (int)HEALTH_BAR_HEIGHT, RED);
    DrawRectangleLines((int)barX, (int)barY, (int)HEALTH_BAR_WIDTH, (int)HEALTH_BAR_HEIGHT, BLACK);
}
