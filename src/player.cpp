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
static const int MAX_JUMPS = 2; // double jump
static const float SLAM_FALL_SPEED = 400.0f; // px/sec, downward burst when a mid-air attack triggers a slam

static const float JUMP_CROUCH_VISUAL_DURATION = 0.08f;    // how long the takeoff crouch frame holds
static const float SLAM_WINDUP_VISUAL_DURATION = 0.1f;     // how long the slam windup frame holds while falling
static const float SLAM_IMPACT_VISUAL_DURATION = 0.15f;    // how long the impact frame holds after landing
static const float SLAM_RECOVERY_TOTAL_DURATION = 0.35f;   // impact + recovery, total time before normal visuals resume

static const float DOUBLE_JUMP_FRAME_DURATION = 0.1f; // seconds each roll frame holds
static const float SLAM_ARM_DELAY = PLAYER_DOUBLE_JUMP_FRAME_COUNT * DOUBLE_JUMP_FRAME_DURATION; // arms once the roll finishes

static const float CAST_ANIM_DURATION = 0.4f; // 4 frames @ 0.1s each

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
    player.jumpCount = 0;
    player.doubleJumpTimer = 0.0f;
    player.jumpVisualTimer = 0.0f;
    player.isSlamming = false;
    player.slamImpactThisFrame = false;
    player.slamRecovering = false;
    player.slamAnimTimer = 0.0f;
    player.castAnim = CastAnim::NONE;
    player.castAnimTimer = 0.0f;
    return player;
}

void PlayerStartDash(Player &player) {
    player.dashTimer = DASH_DURATION;
}

void PlayerStartCast(Player &player, CastAnim anim) {
    if (anim == CastAnim::NONE) return;
    player.castAnim = anim;
    player.castAnimTimer = CAST_ANIM_DURATION;
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
    player.slamImpactThisFrame = false;

    if (player.beerBlinkTimer > 0.0f) {
        player.beerBlinkTimer -= dt;
        if (player.beerBlinkTimer < 0.0f) player.beerBlinkTimer = 0.0f;
    }
    if (player.beerSlowTimer > 0.0f) {
        player.beerSlowTimer -= dt;
        if (player.beerSlowTimer < 0.0f) player.beerSlowTimer = 0.0f;
    }
    player.width = (player.beerSlowTimer > 0.0f) ? player.baseWidth * BEER_BLOAT_SCALE : player.baseWidth;

    if (player.isSlamming) {
        player.velocity.x = 0; // committed straight down until it lands
    } else if (player.dashTimer > 0.0f) {
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

    if (IsKeyPressed(KEY_UP) && !player.isSlamming && player.beerSlowTimer <= 0.0f && player.jumpCount < MAX_JUMPS) {
        player.velocity.y = JUMP_VELOCITY;
        player.onGround = false;
        player.justJumped = true;
        player.jumpCount++;
        player.jumpVisualTimer = 0.0f;
        if (player.jumpCount >= MAX_JUMPS) {
            player.doubleJumpTimer = 0.0f; // starts the roll; slam arms after SLAM_ARM_DELAY
        }
    }

    if (player.jumpCount >= MAX_JUMPS && !player.onGround) {
        player.doubleJumpTimer += dt;
    }
    if (!player.onGround) {
        player.jumpVisualTimer += dt;
    }

    if (IsKeyPressed(KEY_ONE)) player.stance = Stance::MAJOR;
    if (IsKeyPressed(KEY_TWO)) player.stance = Stance::MINOR;

    if (IsKeyPressed(KEY_SPACE) && !player.isAttacking && !player.isSlamming) {
        if (player.onGround) {
            player.isAttacking = true;
            player.attackTimer = ATTACK_DURATION;
            player.attackHasHit = false;
        } else if (player.jumpCount >= MAX_JUMPS && player.doubleJumpTimer >= SLAM_ARM_DELAY) {
            player.isSlamming = true;
            player.slamAnimTimer = 0.0f;
            player.velocity.y = SLAM_FALL_SPEED;
        }
        // else: airborne but not eligible for a slam yet -- input is ignored
    }

    if (player.isSlamming) {
        player.slamAnimTimer += dt;
    }
    if (player.slamRecovering) {
        player.slamAnimTimer += dt;
        if (player.slamAnimTimer >= SLAM_RECOVERY_TOTAL_DURATION) {
            player.slamRecovering = false;
        }
    }

    if (player.isAttacking) {
        player.attackTimer -= dt;
        if (player.attackTimer <= 0.0f) {
            player.isAttacking = false;
            player.attackTimer = 0.0f;
        }
    }

    if (player.castAnim != CastAnim::NONE) {
        player.castAnimTimer -= dt;
        if (player.castAnimTimer <= 0.0f) {
            player.castAnim = CastAnim::NONE;
            player.castAnimTimer = 0.0f;
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
        player.jumpCount = 0;
        player.doubleJumpTimer = 0.0f;
        if (player.isSlamming) {
            player.isSlamming = false;
            player.slamImpactThisFrame = true;
            player.slamRecovering = true;
            player.slamAnimTimer = 0.0f; // now counts the post-landing impact/recovery window
        }
    }
}

static const float SPRITE_BASE_SIZE = 32.0f; // drawn size in virtual pixels at baseWidth

void PlayerDraw(const Player &player, const PlayerSprites &sprites) {
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

    float spriteSize = SPRITE_BASE_SIZE;

    float centerX = player.position.x + player.width / 2.0f;
    float feetY = player.position.y + player.height;

    Texture2D texture = sprites.idle;
    if (player.isAttacking) {
        float t = 1.0f - (player.attackTimer / ATTACK_DURATION); // 0..1 through the swing
        int frameIndex = (int)(t * PLAYER_ATTACK_FRAME_COUNT);
        if (frameIndex >= PLAYER_ATTACK_FRAME_COUNT) frameIndex = PLAYER_ATTACK_FRAME_COUNT - 1;
        if (frameIndex < 0) frameIndex = 0;
        texture = sprites.attack[frameIndex];
    } else if (player.isSlamming) {
        texture = (player.slamAnimTimer < SLAM_WINDUP_VISUAL_DURATION) ? sprites.slam[0] : sprites.slam[1];
    } else if (player.slamRecovering) {
        texture = (player.slamAnimTimer < SLAM_IMPACT_VISUAL_DURATION) ? sprites.slam[2] : sprites.slam[3];
    } else if (player.castAnim != CastAnim::NONE) {
        const Texture2D *castFrames = (player.castAnim == CastAnim::FIREBALL) ? sprites.castFireball : sprites.castFrost;
        float t = 1.0f - (player.castAnimTimer / CAST_ANIM_DURATION); // 0..1 through the cast
        int frameIndex = (int)(t * PLAYER_CAST_FRAME_COUNT);
        if (frameIndex >= PLAYER_CAST_FRAME_COUNT) frameIndex = PLAYER_CAST_FRAME_COUNT - 1;
        if (frameIndex < 0) frameIndex = 0;
        texture = castFrames[frameIndex];
    } else if (!player.onGround && player.jumpCount >= MAX_JUMPS) {
        int frameIndex = (int)(player.doubleJumpTimer / DOUBLE_JUMP_FRAME_DURATION);
        if (frameIndex >= PLAYER_DOUBLE_JUMP_FRAME_COUNT) frameIndex = PLAYER_DOUBLE_JUMP_FRAME_COUNT - 1;
        texture = sprites.doubleJump[frameIndex]; // roll animation for the second jump; holds "unroll" once finished
    } else if (!player.onGround) {
        if (player.jumpVisualTimer < JUMP_CROUCH_VISUAL_DURATION) {
            texture = sprites.jump[0]; // crouch, brief takeoff pose
        } else if (player.velocity.y < -20.0f) {
            texture = sprites.jump[1]; // rising
        } else if (player.velocity.y > 20.0f) {
            texture = sprites.jump[3]; // falling
        } else {
            texture = sprites.jump[2]; // apex
        }
    } else if (player.beerSlowTimer > 0.0f) {
        texture = sprites.bloated;
    }

    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    if (!player.facingRight) src.width = -src.width;

    Rectangle dst = {centerX, feetY, spriteSize, spriteSize};
    Vector2 origin = {spriteSize / 2.0f, spriteSize};

    DrawTexturePro(texture, src, dst, origin, 0.0f, tint);
}

const char *StanceToString(Stance stance) {
    return (stance == Stance::MAJOR) ? "STANCE: MAJOR" : "STANCE: MINOR";
}
