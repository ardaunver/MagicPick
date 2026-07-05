#pragma once
#include "raylib.h"

enum class Stance {
    MAJOR,
    MINOR
};

static const int PLAYER_MAX_HEALTH = 100;
static const int PLAYER_ATTACK_FRAME_COUNT = 4; // windup, swing, impact, recovery
static const int PLAYER_JUMP_FRAME_COUNT = 4;    // crouch, rise, apex, fall
static const int PLAYER_SLAM_FRAME_COUNT = 4;    // windup, swing, impact, recovery
static const int PLAYER_DOUBLE_JUMP_FRAME_COUNT = 4; // tuck, roll, spin, unroll
static const int PLAYER_CAST_FRAME_COUNT = 4;    // charge low, charge high, release, recovery

enum class CastAnim {
    NONE,
    FIREBALL,
    FROST
};

// Bundles every sprite/animation set PlayerDraw needs so its signature
// doesn't grow a new positional Texture2D parameter for every new move.
struct PlayerSprites {
    Texture2D idle;
    Texture2D bloated;
    const Texture2D *attack;       // PLAYER_ATTACK_FRAME_COUNT: windup, swing, impact, recovery
    const Texture2D *jump;         // PLAYER_JUMP_FRAME_COUNT: crouch, rise, apex, fall
    const Texture2D *doubleJump;   // PLAYER_DOUBLE_JUMP_FRAME_COUNT: tuck, roll, spin, unroll
    const Texture2D *slam;         // PLAYER_SLAM_FRAME_COUNT: windup, swing, impact, recovery
    const Texture2D *castFireball; // PLAYER_CAST_FRAME_COUNT
    const Texture2D *castFrost;    // PLAYER_CAST_FRAME_COUNT
};

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
    int jumpCount;        // jumps used since last touching ground (max 2, for double jump)
    float doubleJumpTimer; // time since the double jump (roll) triggered; only counts once jumpCount reaches 2
    float jumpVisualTimer; // time since the current jump/airborne phase started, drives jump frame selection
    bool isSlamming;      // committed to a fast fall after a mid-air attack press
    bool slamImpactThisFrame; // true only on the exact frame a slam lands
    bool slamRecovering;  // brief window after landing showing the impact/recovery slam frames
    float slamAnimTimer;  // time within the current slam sub-phase (falling, then recovering)
    CastAnim castAnim;     // which spell-cast animation is playing, if any
    float castAnimTimer;   // counts down while castAnim != NONE
};

Player PlayerCreate(Vector2 startPos);
void PlayerUpdate(Player &player, float dt, float groundY, float screenWidth);
void PlayerDraw(const Player &player, const PlayerSprites &sprites);
const char *StanceToString(Stance stance);
// Starts a quick dash burst in the player's current facing direction.
void PlayerStartDash(Player &player);
// Starts the cast animation for a spell (fireball or frost); does nothing for CastAnim::NONE.
void PlayerStartCast(Player &player, CastAnim anim);
// The melee swing's hitbox this frame; zero-area rect when not attacking.
Rectangle PlayerGetAttackHitbox(const Player &player);
// Heals 20 HP, blinks green 3 times, and bloats the player (2x width,
// 75% slower movement) for 5 seconds.
void PlayerDrinkBeer(Player &player);
