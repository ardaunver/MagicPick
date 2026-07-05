#include "player.h"
#include <cmath>

static const float MOVE_SPEED = 80.0f;   // px/sec, virtual canvas units
static const float GRAVITY = 400.0f;     // px/sec^2
static const float JUMP_VELOCITY = -160.0f;
static const float ATTACK_DURATION = 0.2f;
static const float ATTACK_WIDTH = 14.0f;
static const float ATTACK_HEIGHT = 8.0f;
static const float DASH_DURATION = 0.15f;
static const float DASH_SPEED = 1280.0f; // covers ~60% of a 320-wide screen in one dash

static const int BEER_HEAL_AMOUNT = 20;
static const float BEER_SLOW_DURATION = 5.0f;
static const float BEER_SLOW_MULTIPLIER = 0.25f; // 75% slower
static const float BEER_BLOAT_SCALE = 2.0f;
static const float BEER_BLINK_DURATION = 0.9f;   // 3 on/off cycles

Player PlayerCreate(Vector2 startPos) {
    Player player;
    player.position = startPos;
    player.velocity = {0, 0};
    player.baseWidth = 12;
    player.width = player.baseWidth;
    player.height = 20;
    player.onGround = false;
    player.stance = Stance::MAJOR;
    player.facingRight = true;
    player.isAttacking = false;
    player.attackTimer = 0.0f;
    player.flashTimer = 0.0f;
    player.health = PLAYER_MAX_HEALTH;
    player.dashTimer = 0.0f;
    player.justJumped = false;
    player.attackHasHit = false;
    player.beerBlinkTimer = 0.0f;
    player.beerSlowTimer = 0.0f;
    return player;
}

void PlayerStartDash(Player &player) {
    player.dashTimer = DASH_DURATION;
}

void PlayerDrinkBeer(Player &player) {
    player.health += BEER_HEAL_AMOUNT;
    if (player.health > PLAYER_MAX_HEALTH) player.health = PLAYER_MAX_HEALTH;
    player.beerBlinkTimer = BEER_BLINK_DURATION;
    player.beerSlowTimer = BEER_SLOW_DURATION;
}

Rectangle PlayerGetAttackHitbox(const Player &player) {
    if (!player.isAttacking) return {0, 0, 0, 0};
    float hitboxX = player.facingRight
        ? player.position.x + player.width
        : player.position.x - ATTACK_WIDTH;
    return {hitboxX, player.position.y + player.height * 0.3f, ATTACK_WIDTH, ATTACK_HEIGHT};
}

void PlayerUpdate(Player &player, float dt, float groundY, float screenWidth) {
    player.justJumped = false;

    if (player.beerBlinkTimer > 0.0f) {
        player.beerBlinkTimer -= dt;
        if (player.beerBlinkTimer < 0.0f) player.beerBlinkTimer = 0.0f;
    }
    if (player.beerSlowTimer > 0.0f) {
        player.beerSlowTimer -= dt;
        if (player.beerSlowTimer < 0.0f) player.beerSlowTimer = 0.0f;
    }
    player.width = (player.beerSlowTimer > 0.0f) ? player.baseWidth * BEER_BLOAT_SCALE : player.baseWidth;

    if (player.dashTimer > 0.0f) {
        float dashDir = player.facingRight ? 1.0f : -1.0f;
        player.velocity.x = DASH_SPEED * dashDir;
        player.dashTimer -= dt;
        if (player.dashTimer < 0.0f) player.dashTimer = 0.0f;
    } else {
        float moveSpeed = (player.beerSlowTimer > 0.0f) ? MOVE_SPEED * BEER_SLOW_MULTIPLIER : MOVE_SPEED;
        player.velocity.x = 0;
        if (IsKeyDown(KEY_LEFT))  player.velocity.x -= moveSpeed;
        if (IsKeyDown(KEY_RIGHT)) player.velocity.x += moveSpeed;

        if (player.velocity.x < 0) player.facingRight = false;
        if (player.velocity.x > 0) player.facingRight = true;
    }

    if (IsKeyPressed(KEY_UP) && player.onGround) {
        player.velocity.y = JUMP_VELOCITY;
        player.onGround = false;
        player.justJumped = true;
    }

    if (IsKeyPressed(KEY_ONE)) player.stance = Stance::MAJOR;
    if (IsKeyPressed(KEY_TWO)) player.stance = Stance::MINOR;

    if (IsKeyPressed(KEY_SPACE) && !player.isAttacking) {
        player.isAttacking = true;
        player.attackTimer = ATTACK_DURATION;
        player.attackHasHit = false;
    }

    if (player.isAttacking) {
        player.attackTimer -= dt;
        if (player.attackTimer <= 0.0f) {
            player.isAttacking = false;
            player.attackTimer = 0.0f;
        }
    }

    if (player.flashTimer > 0.0f) {
        player.flashTimer -= dt;
        if (player.flashTimer < 0.0f) player.flashTimer = 0.0f;
    }

    player.velocity.y += GRAVITY * dt;

    player.position.x += player.velocity.x * dt;
    player.position.y += player.velocity.y * dt;

    if (player.position.x < 0) player.position.x = 0;
    if (player.position.x + player.width > screenWidth) player.position.x = screenWidth - player.width;

    if (player.position.y + player.height >= groundY) {
        player.position.y = groundY - player.height;
        player.velocity.y = 0;
        player.onGround = true;
    }
}

static const float SPRITE_BASE_SIZE = 32.0f; // drawn size in virtual pixels at baseWidth
static const float ATTACK_SWING_DEGREES = 30.0f;

void PlayerDraw(const Player &player, Texture2D texture) {
    bool beerBlinkOn = false;
    if (player.beerBlinkTimer > 0.0f) {
        float elapsed = BEER_BLINK_DURATION - player.beerBlinkTimer;
        int phase = (int)(elapsed / (BEER_BLINK_DURATION / 6.0f));
        beerBlinkOn = (phase % 2 == 0);
    }

    Color tint;
    if (player.flashTimer > 0.0f) {
        tint = ORANGE;
    } else if (beerBlinkOn) {
        tint = GREEN;
    } else {
        tint = WHITE;
    }

    float scale = player.width / player.baseWidth; // 1.0 normally, 2.0 while bloated
    float spriteSize = SPRITE_BASE_SIZE * scale;

    float centerX = player.position.x + player.width / 2.0f;
    float feetY = player.position.y + player.height;

    // Swing the whole sprite around the feet during an attack, since the
    // guitar-as-club motion is now baked into the art rather than animated
    // bone-by-bone.
    float swingDeg = 0.0f;
    if (player.isAttacking) {
        float t = 1.0f - (player.attackTimer / ATTACK_DURATION);
        swingDeg = sinf(t * PI) * ATTACK_SWING_DEGREES;
    }
    float rotation = player.facingRight ? swingDeg : -swingDeg;

    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    if (!player.facingRight) src.width = -src.width;

    Rectangle dst = {centerX, feetY, spriteSize, spriteSize};
    Vector2 origin = {spriteSize / 2.0f, spriteSize};

    DrawTexturePro(texture, src, dst, origin, rotation, tint);
}

const char *StanceToString(Stance stance) {
    return (stance == Stance::MAJOR) ? "STANCE: MAJOR" : "STANCE: MINOR";
}
