#include "raylib.h"
#include "player.h"
#include "chord_system.h"
#include "enemy.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

#if defined(PLATFORM_IOS)
#include "touch_controls.h"
#endif

// raylib changed DrawCircleGradient's signature from (int,int,float,Color,Color)
// to (Vector2,float,Color,Color) around version 6.0. Desktop/Windows here
// build against 5.5 (old signature); the iOS fork is on a newer snapshot
// (new signature). This wrapper hides the difference at the two call sites.
static void DrawCircleGradientCompat(float x, float y, float radius, Color inner, Color outer) {
#if defined(PLATFORM_IOS)
    DrawCircleGradient(Vector2{x, y}, radius, inner, outer);
#else
    DrawCircleGradient((int)x, (int)y, radius, inner, outer);
#endif
}

static const int VIRTUAL_WIDTH = 320;
static const int VIRTUAL_HEIGHT = 180;
static const int SCALE = 3;
static const int WINDOW_WIDTH = VIRTUAL_WIDTH * SCALE;
static const int WINDOW_HEIGHT = VIRTUAL_HEIGHT * SCALE;
static const int ENEMY_COUNT = 2;

struct Fireball {
    Vector2 position;
    bool active;
    bool facingRight;
    float lifetime;
};

static const float FIREBALL_SPEED = 150.0f;
static const float FIREBALL_LIFETIME = 5.0f; // safety net; it despawns off-screen well before this
static const float FIREBALL_RADIUS = 7.0f;
static const float FIREBALL_SPRITE_WIDTH = 20.0f; // drawn size in virtual pixels; source art is a 1.5:1 aspect icon
static const float FIREBALL_SPRITE_HEIGHT = FIREBALL_SPRITE_WIDTH * (320.0f / 480.0f);

static const int MELEE_DAMAGE = ENEMY_MAX_HEALTH / 4; // 4 base attacks to kill, at full tune
static const int FIREBALL_DAMAGE = ENEMY_MAX_HEALTH;  // one-shot, at full tune
static const int FIREBALL_BURN_DAMAGE = 20;           // extra damage-over-time, at full tune
static const float FIREBALL_BURN_DURATION = 2.0f;

static const int SLAM_DAMAGE = ENEMY_MAX_HEALTH / 3; // mid-air attack -> ground slam, at full tune
static const float SLAM_RADIUS = 24.0f; // virtual pixels, AoE around the player on landing

struct FrostBlast {
    Vector2 position;
    bool active;
    float timer;
};

static const int FROST_BLAST_DAMAGE = ENEMY_MAX_HEALTH / 2; // 50, at full tune
static const float FROST_BLAST_KNOCKBACK_FORCE = 30.0f;
static const float FROST_BLAST_ANIM_DURATION = 0.4f;

// A beer mug that pops out of a slain slime, hops through a little arc,
// lands and sits on the floor, and is collected (heals + bloats the player)
// if the player walks into it before it despawns.
struct BeerDrop {
    Vector2 position;
    Vector2 velocity;
    bool active;
    float groundTimer; // counts down once landed; only meaningful after landing
};

static const float BEER_POP_SPEED = 45.0f;
static const float BEER_POP_GRAVITY = 220.0f;
static const float BEER_DROP_SIZE = 8.0f; // matches the DrawBeerIcon size passed below
static const float BEER_GROUND_DURATION = 5.0f; // despawns this long after landing if uncollected
static const float FLOATING_TEXT_DURATION = 1.2f;

static const float TUNE_COOLDOWN_DURATION = 10.0f;

static const float PIANO_KEY_WIDTH = 26.0f;
static const float PIANO_KEY_HEIGHT = 14.0f;
static const float PIANO_KEY_GAP = 2.0f;

static const int HUD_PANEL_HEIGHT = 26;
static const int HUD_PANEL_FADE_WIDTH = 16;
static const int PIANO_PANEL_WIDTH =
    (int)(NOTE_KEY_COUNT * PIANO_KEY_WIDTH + (NOTE_KEY_COUNT - 1) * PIANO_KEY_GAP) + 4;
static const int PIANO_PANEL_HEIGHT = (int)PIANO_KEY_HEIGHT + 4;
static const int PANEL_FEATHER_MARGIN = 6;

static const int STANCE_FONT_SIZE = 8;
static const int SPELL_NAME_FONT_SIZE = 8;
static const int BAR_LABEL_FONT_SIZE = 7;

static const int SCROLL_BUTTON_SIZE = 16;
static const Color PARCHMENT_COLOR = {222, 184, 135, 255};

static const int STRING_COUNT = 6;
static const char *STRING_LABELS[STRING_COUNT] = {"E", "B", "G", "D", "A", "E"};
static const float TUNE_GAME_TOLERANCE = 0.04f;
static const float TUNE_GAME_DURATION = 7.0f;
static const float COUNTDOWN_POP_DURATION = 0.3f;
static const float TUNE_SPEED_BASE = 0.4f;    // fraction of the track per second, at the start of a hold
static const float TUNE_SPEED_GROWTH = 2.6f;  // exponential growth rate while held continuously

static const float HEALTH_BAR_WIDTH = 60.0f;
static const float HEALTH_BAR_HEIGHT = 8.0f;
static const float TUNE_BAR_HEIGHT = 8.0f;
static const float TUNE_BAR_GAP = 2.0f;
static const float TUNE_DURATION_SECONDS = 600.0f; // 10 minutes to fully decay
static const float TUNE_DECAY_RATE = 2.302585f / TUNE_DURATION_SECONDS; // ln(10)/600, gentler early slope

static const float AMBIENT_DARK_ALPHA = 0.7f;  // 70% dark overlay -> 30% ambient brightness, applied uniformly
static const float LIGHT_RADIUS = 110.0f;
static const float LIGHT_DURATION = 60.0f;
static const float LIGHT_FADE_DURATION = 3.0f;
static const float LIGHT_PULSE_PERIOD = 1.25f; // seconds per brightness pulse cycle
static const float LIGHT_PULSE_MIN = 0.75f;   // dimmest point of the pulse, as a fraction of full brightness
static const float FROST_BLAST_RADIUS = LIGHT_RADIUS / 3.0f;
static const float FROST_LIGHT_MAX_ALPHA = 0.5f; // peak strength of the frost blast's area-indicator glow

static const float SPELL_NAME_DISPLAY_TIME = 1.5f;

static const float NOTE_SOUND_DURATION = 0.6f;
static const float NOTE_STRING_DAMPING = 0.996f;

// Karplus-Strong plucked-string synthesis: a noise burst is fed through a
// short decaying delay loop, giving a guitar-like pluck instead of a pure tone.
static Sound GenerateNoteSound(float frequency) {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * NOTE_SOUND_DURATION);
    int bufferLength = (int)(sampleRate / frequency);

    std::vector<float> ringBuffer(bufferLength);
    for (int i = 0; i < bufferLength; i++) {
        ringBuffer[i] = (float)GetRandomValue(-1000, 1000) / 1000.0f;
    }

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    int p = 0;
    for (int i = 0; i < sampleCount; i++) {
        float value = ringBuffer[p];
        int next = (p + 1) % bufferLength;
        ringBuffer[p] = NOTE_STRING_DAMPING * 0.5f * (ringBuffer[p] + ringBuffer[next]);
        p = next;

        float envelope = 1.0f - (float)i / sampleCount; // overall fade-out, avoids a click at the end
        samples[i] = (short)(value * envelope * 12000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static const float JUMP_SOUND_DURATION = 0.15f;

// A quick upward pitch-swept blip for the jump action.
static Sound GenerateJumpSound() {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * JUMP_SOUND_DURATION);

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleRate;
        float frequency = 300.0f + 500.0f * (t / JUMP_SOUND_DURATION); // sweep 300 -> 800 Hz
        float envelope = 1.0f - (float)i / sampleCount;
        samples[i] = (short)(sinf(2.0f * PI * frequency * t) * envelope * 10000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static const float ENEMY_DEATH_SOUND_DURATION = 0.25f;

// A short descending "poof" (tone sweeping down mixed with noise) for any enemy's death.
static Sound GenerateEnemyDeathSound() {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * ENEMY_DEATH_SOUND_DURATION);

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleRate;
        float frequency = 500.0f - 400.0f * (t / ENEMY_DEATH_SOUND_DURATION); // sweep 500 -> 100 Hz
        float tone = sinf(2.0f * PI * frequency * t);
        float noise = (float)GetRandomValue(-1000, 1000) / 1000.0f;
        float envelope = 1.0f - (float)i / sampleCount;
        samples[i] = (short)((tone * 0.6f + noise * 0.4f) * envelope * 9000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static const float ENEMY_HIT_SOUND_DURATION = 0.08f;

// A short, punchy thwack for landing a hit that doesn't kill the enemy --
// distinct from the death "poof" below (higher, tighter, noise-forward).
static Sound GenerateEnemyHitSound() {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * ENEMY_HIT_SOUND_DURATION);

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleRate;
        float thump = sinf(2.0f * PI * 150.0f * t);
        float noise = (float)GetRandomValue(-1000, 1000) / 1000.0f;
        float envelope = powf(1.0f - (float)i / sampleCount, 3.0f); // fast decay
        samples[i] = (short)((thump * 0.4f + noise * 0.6f) * envelope * 11000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static const float PLAYER_DEATH_SOUND_DURATION = 0.6f;

// A short, harsh male pain scream: a sawtooth cry (richer harmonics than a
// sine, reads more like a shout) with a fast wobble, dropping in pitch and
// mixed with noise for rasp.
static Sound GeneratePlayerDeathSound() {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * PLAYER_DEATH_SOUND_DURATION);

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleRate;
        float progress = t / PLAYER_DEATH_SOUND_DURATION;

        float baseFreq = 480.0f - 320.0f * progress; // cry dropping from 480Hz to 160Hz
        float vibrato = sinf(2.0f * PI * 28.0f * t) * 18.0f; // fast waver
        float frequency = baseFreq + vibrato;

        float phase = frequency * t;
        float saw = 2.0f * (phase - floorf(phase + 0.5f)); // sawtooth, harsher than a sine

        float noise = (float)GetRandomValue(-1000, 1000) / 1000.0f;

        float envelope;
        if (progress < 0.05f) {
            envelope = progress / 0.05f; // sharp intake
        } else {
            envelope = powf(1.0f - (progress - 0.05f) / 0.95f, 1.3f);
        }

        float value = (saw * 0.65f + noise * 0.35f) * envelope;
        samples[i] = (short)(value * 13000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static const float PLAYER_HIT_SOUND_DURATION = 0.15f;

// A short downward-pitched thud+noise "oof" for the player getting hit --
// distinct from both the enemy hit thwack and the death scream.
static Sound GeneratePlayerHitSound() {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * PLAYER_HIT_SOUND_DURATION);

    short *samples = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleRate;
        float frequency = 220.0f - 140.0f * (t / PLAYER_HIT_SOUND_DURATION); // downward thud 220 -> 80Hz
        float tone = sinf(2.0f * PI * frequency * t);
        float noise = (float)GetRandomValue(-1000, 1000) / 1000.0f;
        float envelope = powf(1.0f - (float)i / sampleCount, 2.0f);
        samples[i] = (short)((tone * 0.55f + noise * 0.45f) * envelope * 11000.0f);
    }

    Wave wave = {0};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

// A rectangular vignette: opaque core, edges feathering to transparent -- the
// same idea as the Beam spell's circular radial falloff, but for a rectangle.
// HUD panels are static, so this is precomputed once instead of per frame.
static Texture2D CreatePanelMask(int width, int height, int margin) {
    std::vector<Color> pixels(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int distToEdge = x;
            if (width - 1 - x < distToEdge) distToEdge = width - 1 - x;
            if (y < distToEdge) distToEdge = y;
            if (height - 1 - y < distToEdge) distToEdge = height - 1 - y;

            float alpha = (distToEdge >= margin) ? 1.0f : (float)distToEdge / margin;
            if (alpha < 0.0f) alpha = 0.0f;

            pixels[y * width + x] = Color{255, 255, 255, (unsigned char)(alpha * 255)};
        }
    }
    Image image = {pixels.data(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return LoadTextureFromImage(image);
}


// An icy burst expanding outward from the center, fading as it grows. A
// radial gradient (pale cyan core -> deep blue rim) sells the "frost" feel.
static void DrawFrostBlast(Vector2 center, float radius, float progress) {
    float currentRadius = radius * progress;
    float alpha = 1.0f - progress;

    Color innerColor = Fade(Color{210, 245, 255, 255}, alpha * 0.7f);
    Color outerColor = Fade(Color{20, 90, 220, 255}, alpha * 0.6f);
    DrawCircleGradientCompat(center.x, center.y, currentRadius, innerColor, outerColor);
    DrawCircleLines((int)center.x, (int)center.y, currentRadius, Fade(Color{130, 200, 255, 255}, alpha * 0.9f));

    int shardCount = 8;
    for (int i = 0; i < shardCount; i++) {
        float angle = i * (2.0f * PI / shardCount);
        Vector2 shardPos = {center.x + cosf(angle) * currentRadius, center.y + sinf(angle) * currentRadius};
        DrawCircleV(shardPos, 2.0f, Fade(Color{50, 130, 230, 255}, alpha));
    }
}

// A small beer mug: amber body with a foam cap and a handle.
static void DrawBeerIcon(Vector2 center, float size, float alpha) {
    float w = size;
    float h = size * 1.2f;

    Rectangle body = {center.x - w / 2.0f, center.y - h / 2.0f + h * 0.2f, w, h * 0.8f};
    DrawRectangleRec(body, Fade(Color{255, 179, 0, 255}, alpha));
    DrawRectangle((int)(body.x + w * 0.18f), (int)body.y, (int)(w * 0.2f), (int)body.height, Fade(Color{255, 224, 130, 255}, alpha));
    DrawRectangleLinesEx(body, 1.0f, Fade(BLACK, alpha));

    Rectangle foam = {center.x - w / 2.0f, center.y - h / 2.0f, w, h * 0.25f};
    DrawRectangleRounded(foam, 0.6f, 4, Fade(RAYWHITE, alpha));
    DrawRectangleRoundedLinesEx(foam, 0.6f, 4, 1.0f, Fade(BLACK, alpha));

    Rectangle handle = {center.x + w / 2.0f - 1.0f, center.y - h * 0.15f, w * 0.35f, h * 0.5f};
    DrawRectangleRoundedLinesEx(handle, 0.6f, 4, 1.5f, Fade(BLACK, alpha));
}

// A small ghost: rounded pale body, a scalloped (wavy) hem, dot eyes -- the
// "soul" icon for the score counter.
static void DrawGhostIcon(Vector2 center, float size) {
    Color body = Color{225, 228, 235, 255};
    Color outline = Color{110, 115, 125, 255};
    float w = size;
    float h = size * 1.15f;

    Rectangle bodyRect = {center.x - w / 2.0f, center.y - h / 2.0f, w, h * 0.7f};
    DrawRectangleRounded(bodyRect, 0.9f, 8, body);

    float hemY = bodyRect.y + bodyRect.height;
    int waveCount = 3;
    float waveR = w / (waveCount * 2.2f);
    for (int i = 0; i < waveCount; i++) {
        float wx = bodyRect.x + w * (i + 0.5f) / waveCount;
        DrawCircle((int)wx, (int)hemY, waveR, body);
    }
    DrawRectangleRoundedLinesEx(bodyRect, 0.9f, 8, 1.0f, outline);

    DrawCircle((int)(center.x - w * 0.18f), (int)(center.y - h * 0.08f), w * 0.09f, BLACK);
    DrawCircle((int)(center.x + w * 0.18f), (int)(center.y - h * 0.08f), w * 0.09f, BLACK);
}

// On iOS, the platform's own Objective-C main() (rcore_ios_main.m) owns the
// app lifecycle and calls this on its own dispatched thread instead of us
// providing a normal main(). Desktop/Windows keep a plain main().
#if defined(PLATFORM_IOS)
extern "C" int raylib_main(int argc, char *argv[]) {
#else
int main() {
#endif
    // Without this, Windows displays with scaling above 100% (very common on
    // laptops) can make the window come out smaller than requested.
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Magic Pick");
    SetExitKey(KEY_NULL); // ESC is used to close UI panels, not quit the app
    InitAudioDevice();
    SetTargetFPS(60);

#if defined(PLATFORM_IOS)
    // BeginTextureMode/EndTextureMode (render-to-texture) renders a black
    // screen on this iOS fork -- confirmed via isolated repro, a bug in the
    // platform layer itself, not our drawing code. BeginMode2D with a zoomed
    // camera achieves the same "320x180 virtual canvas, scaled up" pixel-art
    // look without ever creating an offscreen framebuffer, sidestepping the
    // bug entirely. Revisit if/when the fork's render-texture support is fixed.
    Camera2D camera = {0};
    camera.zoom = (float)SCALE;
    TouchControlsInit((float)VIRTUAL_WIDTH, (float)VIRTUAL_HEIGHT);
#else
    RenderTexture2D canvas = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
    Rectangle canvasSrc = {0, 0, (float)VIRTUAL_WIDTH, -(float)VIRTUAL_HEIGHT};
    Rectangle canvasDst = {0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT};
#endif

    Texture2D pianoPanelMask = CreatePanelMask(PIANO_PANEL_WIDTH, PIANO_PANEL_HEIGHT, PANEL_FEATHER_MARGIN);
    // Resolve relative to the executable's own folder, not the working
    // directory, so double-clicking the packaged app finds its assets.
    const char *caveTexturePath = TextFormat("%sassets/cave.png", GetApplicationDirectory());
    Texture2D caveTexture = LoadTexture(caveTexturePath);
    Rectangle caveSrc = {0, 0, (float)caveTexture.width, (float)caveTexture.height};
    Rectangle caveDst = {0, 0, (float)VIRTUAL_WIDTH, (float)VIRTUAL_HEIGHT};

    const char *playerTexturePath = TextFormat("%sassets/player/idle.png", GetApplicationDirectory());
    Texture2D playerTexture = LoadTexture(playerTexturePath);

    const char *playerAttackFrameFiles[PLAYER_ATTACK_FRAME_COUNT] = {
        "player/base_attack/1_windup.png",
        "player/base_attack/2_swing.png",
        "player/base_attack/3_impact.png",
        "player/base_attack/4_recovery.png",
    };
    Texture2D playerAttackFrames[PLAYER_ATTACK_FRAME_COUNT];
    for (int i = 0; i < PLAYER_ATTACK_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerAttackFrameFiles[i]);
        playerAttackFrames[i] = LoadTexture(path);
    }

    const char *playerBloatedTexturePath = TextFormat("%sassets/player/bloated.png", GetApplicationDirectory());
    Texture2D playerBloatedTexture = LoadTexture(playerBloatedTexturePath);

    const char *playerJumpFrameFiles[PLAYER_JUMP_FRAME_COUNT] = {
        "player/jump/1_crouch.png",
        "player/jump/2_rise.png",
        "player/jump/3_apex.png",
        "player/jump/4_fall.png",
    };
    Texture2D playerJumpFrames[PLAYER_JUMP_FRAME_COUNT];
    for (int i = 0; i < PLAYER_JUMP_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerJumpFrameFiles[i]);
        playerJumpFrames[i] = LoadTexture(path);
    }

    const char *playerSlamFrameFiles[PLAYER_SLAM_FRAME_COUNT] = {
        "player/slam/1_windup.png",
        "player/slam/2_swing.png",
        "player/slam/3_impact.png",
        "player/slam/4_recovery.png",
    };
    Texture2D playerSlamFrames[PLAYER_SLAM_FRAME_COUNT];
    for (int i = 0; i < PLAYER_SLAM_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerSlamFrameFiles[i]);
        playerSlamFrames[i] = LoadTexture(path);
    }

    const char *playerDoubleJumpFrameFiles[PLAYER_DOUBLE_JUMP_FRAME_COUNT] = {
        "player/double_jump/1_tuck.png",
        "player/double_jump/2_roll.png",
        "player/double_jump/3_spin.png",
        "player/double_jump/4_unroll.png",
    };
    Texture2D playerDoubleJumpFrames[PLAYER_DOUBLE_JUMP_FRAME_COUNT];
    for (int i = 0; i < PLAYER_DOUBLE_JUMP_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerDoubleJumpFrameFiles[i]);
        playerDoubleJumpFrames[i] = LoadTexture(path);
    }

    const char *playerCastFireballFrameFiles[PLAYER_CAST_FRAME_COUNT] = {
        "player/cast_fireball/1_charge_low.png",
        "player/cast_fireball/2_charge_high.png",
        "player/cast_fireball/3_release.png",
        "player/cast_fireball/4_recovery.png",
    };
    Texture2D playerCastFireballFrames[PLAYER_CAST_FRAME_COUNT];
    for (int i = 0; i < PLAYER_CAST_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerCastFireballFrameFiles[i]);
        playerCastFireballFrames[i] = LoadTexture(path);
    }

    const char *playerCastFrostFrameFiles[PLAYER_CAST_FRAME_COUNT] = {
        "player/cast_frost/1_gather.png",
        "player/cast_frost/2_swirl.png",
        "player/cast_frost/3_release.png",
        "player/cast_frost/4_recovery.png",
    };
    Texture2D playerCastFrostFrames[PLAYER_CAST_FRAME_COUNT];
    for (int i = 0; i < PLAYER_CAST_FRAME_COUNT; i++) {
        const char *path = TextFormat("%sassets/%s", GetApplicationDirectory(), playerCastFrostFrameFiles[i]);
        playerCastFrostFrames[i] = LoadTexture(path);
    }

    const char *fireballSpritePath = TextFormat("%sassets/spells/fireball.png", GetApplicationDirectory());
    Texture2D fireballTexture = LoadTexture(fireballSpritePath);

    PlayerSprites playerSprites = {
        playerTexture, playerBloatedTexture, playerAttackFrames, playerJumpFrames,
        playerDoubleJumpFrames, playerSlamFrames, playerCastFireballFrames, playerCastFrostFrames
    };

    float groundY = VIRTUAL_HEIGHT - 20.0f;
    Player player = PlayerCreate({(float)VIRTUAL_WIDTH / 2, groundY - 20});
    Enemy enemies[ENEMY_COUNT];
    for (int i = 0; i < ENEMY_COUNT; i++) {
        enemies[i] = EnemyCreate(groundY, (float)VIRTUAL_WIDTH);
    }

    const NoteKeyBinding *bindings = GetNoteKeyBindings();
    Sound noteSounds[NOTE_KEY_COUNT];
    for (int i = 0; i < NOTE_KEY_COUNT; i++) {
        noteSounds[i] = GenerateNoteSound(NoteFrequency(bindings[i].note));
    }
    Sound jumpSound = GenerateJumpSound();
    Sound enemyDeathSound = GenerateEnemyDeathSound();
    Sound enemyHitSound = GenerateEnemyHitSound();
    Sound playerDeathSound = GeneratePlayerDeathSound();
    Sound playerHitSound = GeneratePlayerHitSound();

    ChordBurst chordBurst = ChordBurstCreate();
    Fireball fireball = {{0, 0}, false, true, 0.0f};
    FrostBlast frostBlast = {{0, 0}, false, 0.0f};
    BeerDrop beerDrop = {{0, 0}, {0, 0}, false, 0.0f};
    float healTextTimer = 0.0f;
    float bloatTextTimer = 0.0f;

    int soulScore = 0;
    float soulTextTimer = 0.0f;
    Vector2 soulTextPosition = {0, 0};
    int soulTextAmount = 0;

    bool lightActive = false;
    float lightTimer = 0.0f;

    float tuneTimer = 0.0f;

    Spell lastCastSpell = Spell::NONE;
    float spellNameTimer = 0.0f;

    bool showSpellInfo = false;
    Rectangle scrollButtonRect = {
        4, HUD_PANEL_HEIGHT + 4, (float)SCROLL_BUTTON_SIZE, (float)SCROLL_BUTTON_SIZE
    };

    bool showTuneGame = false;
    Rectangle tuneButtonRect = {
        4 + SCROLL_BUTTON_SIZE + 4, HUD_PANEL_HEIGHT + 4, (float)SCROLL_BUTTON_SIZE, (float)SCROLL_BUTTON_SIZE
    };
    float stringTune[STRING_COUNT];
    for (int i = 0; i < STRING_COUNT; i++) {
        stringTune[i] = GetRandomValue(10, 90) / 100.0f; // start off-center
    }
    int selectedString = 0;
    float tuneGameTimer = 0.0f;
    float tuneHoldTime = 0.0f;
    int tuneHeldDirection = 0;
    float tuneCooldownTimer = 0.0f;

    bool showSettings = false;
    Rectangle settingsButtonRect = {
        4 + 2 * (SCROLL_BUTTON_SIZE + 4), HUD_PANEL_HEIGHT + 4, (float)SCROLL_BUTTON_SIZE, (float)SCROLL_BUTTON_SIZE
    };
    bool hostilityDisabled = false; // temporary testing toggle, lives in the settings panel

    int lastCountdownSecond = -1;
    float countdownPopTimer = 0.0f;

    bool gameOver = false;
    bool requestClose = false;

    bool showTitleScreen = true;
    float titleBobTimer = 0.0f;

    auto resetGame = [&]() {
        player = PlayerCreate({(float)VIRTUAL_WIDTH / 2, groundY - 20});
        for (int i = 0; i < ENEMY_COUNT; i++) {
            enemies[i] = EnemyCreate(groundY, (float)VIRTUAL_WIDTH);
        }
        fireball = {{0, 0}, false, true, 0.0f};
        frostBlast = {{0, 0}, false, 0.0f};
        beerDrop = {{0, 0}, {0, 0}, false, 0.0f};
        healTextTimer = 0.0f;
        bloatTextTimer = 0.0f;
        soulScore = 0;
        soulTextTimer = 0.0f;
        chordBurst = ChordBurstCreate();
        lightActive = false;
        lightTimer = 0.0f;
        tuneTimer = 0.0f;
        lastCastSpell = Spell::NONE;
        spellNameTimer = 0.0f;
        showSpellInfo = false;
        showTuneGame = false;
        showSettings = false;
        tuneCooldownTimer = 0.0f;
        gameOver = false;
    };

    std::vector<Color> lightMaskPixels(VIRTUAL_WIDTH * VIRTUAL_HEIGHT, Color{0, 0, 0, 0});
    Image lightMaskImageDesc = {
        lightMaskPixels.data(), VIRTUAL_WIDTH, VIRTUAL_HEIGHT, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D lightMaskTexture = LoadTextureFromImage(lightMaskImageDesc);

    while (!WindowShouldClose() && !requestClose) {
        float dt = GetFrameTime();

#if defined(PLATFORM_IOS)
        // Fit the 320x180 virtual canvas to whatever the real screen size
        // turns out to be (device/orientation dependent, and this fork's
        // reported size has been inconsistent) rather than assuming a fixed
        // window size like desktop can. Letterboxes on one axis if the
        // screen's aspect ratio isn't exactly 16:9.
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        camera.zoom = fminf(screenW / VIRTUAL_WIDTH, screenH / VIRTUAL_HEIGHT);
        camera.offset = {
            (screenW - VIRTUAL_WIDTH * camera.zoom) / 2.0f,
            (screenH - VIRTUAL_HEIGHT * camera.zoom) / 2.0f
        };
        TouchControlsUpdate(dt, camera);
#endif

        float tuneValue = 100.0f * expf(-TUNE_DECAY_RATE * tuneTimer);
        if (tuneTimer >= TUNE_DURATION_SECONDS) tuneValue = 0.0f;

        Rectangle tuneGameRect = {20, 20, (float)(VIRTUAL_WIDTH - 40), 124};
        Rectangle stringTrackRect[STRING_COUNT];
        for (int i = 0; i < STRING_COUNT; i++) {
            float rowY = tuneGameRect.y + 20 + i * 16;
            stringTrackRect[i] = {tuneGameRect.x + 30, rowY + 4, tuneGameRect.width - 40, 6};
        }

        Rectangle infoRect = {20, 40, (float)(VIRTUAL_WIDTH - 40), 96};
        Rectangle infoCloseBtn = {infoRect.x + infoRect.width - 16, infoRect.y + 3, 12, 12};

        Rectangle settingsRect = {20, 20, (float)(VIRTUAL_WIDTH - 40), 147};
        Rectangle settingsCloseBtn = {settingsRect.x + settingsRect.width - 16, settingsRect.y + 3, 12, 12};
        Rectangle hostilityToggleRect = {settingsRect.x + 10, settingsRect.y + 127, settingsRect.width - 20, 14};

        Rectangle retryButtonRect = {VIRTUAL_WIDTH / 2.0f - 50, VIRTUAL_HEIGHT / 2.0f + 10, 45, 18};
        Rectangle closeButtonRect = {VIRTUAL_WIDTH / 2.0f + 5, VIRTUAL_HEIGHT / 2.0f + 10, 45, 18};

        Rectangle titleStartButtonRect = {VIRTUAL_WIDTH / 2.0f - 50, VIRTUAL_HEIGHT / 2.0f + 20, 45, 18};
        Rectangle titleCloseButtonRect = {VIRTUAL_WIDTH / 2.0f + 5, VIRTUAL_HEIGHT / 2.0f + 20, 45, 18};

        if (showTitleScreen) {
            titleBobTimer += dt;

            Vector2 mouseVirtual = {GetMousePosition().x / SCALE, GetMousePosition().y / SCALE};
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouseVirtual, titleStartButtonRect)) {
                    showTitleScreen = false;
                } else if (CheckCollisionPointRec(mouseVirtual, titleCloseButtonRect)) {
                    requestClose = true;
                }
            }
        } else if (showTuneGame) {
            // Tuning minigame: pauses the world and takes over the arrow
            // keys. Holding left/right moves the indicator continuously,
            // and its speed ramps up exponentially the longer it's held --
            // short taps stay controllable, holding gets away from you
            // fast, like an over-sensitive tuning peg. Pressing "t" again
            // closes it early but still scores like a normal timeout --
            // there's no free cancel.
            tuneGameTimer -= dt;

            int displaySeconds = (int)ceilf(tuneGameTimer);
            if (displaySeconds < 0) displaySeconds = 0;
            if (displaySeconds != lastCountdownSecond) {
                lastCountdownSecond = displaySeconds;
                countdownPopTimer = COUNTDOWN_POP_DURATION;
            }
            if (countdownPopTimer > 0.0f) {
                countdownPopTimer -= dt;
                if (countdownPopTimer < 0.0f) countdownPopTimer = 0.0f;
            }

            if (IsKeyPressed(KEY_T)) {
                tuneGameTimer = 0.0f;
            }

            if (IsKeyPressed(KEY_UP)) {
                selectedString = (selectedString - 1 + STRING_COUNT) % STRING_COUNT;
                tuneHoldTime = 0.0f;
                tuneHeldDirection = 0;
            }
            if (IsKeyPressed(KEY_DOWN)) {
                selectedString = (selectedString + 1) % STRING_COUNT;
                tuneHoldTime = 0.0f;
                tuneHeldDirection = 0;
            }

            int direction = 0;
            if (IsKeyDown(KEY_LEFT)) direction = -1;
            else if (IsKeyDown(KEY_RIGHT)) direction = 1;

            if (direction != 0) {
                if (direction == tuneHeldDirection) {
                    tuneHoldTime += dt;
                } else {
                    tuneHoldTime = 0.0f;
                    tuneHeldDirection = direction;
                }
                float speed = TUNE_SPEED_BASE * expf(TUNE_SPEED_GROWTH * tuneHoldTime);
                stringTune[selectedString] += direction * speed * dt;
                if (stringTune[selectedString] < 0.0f) stringTune[selectedString] = 0.0f;
                if (stringTune[selectedString] > 1.0f) stringTune[selectedString] = 1.0f;
            } else {
                tuneHoldTime = 0.0f;
                tuneHeldDirection = 0;
            }

            bool allInTune = true;
            for (int i = 0; i < STRING_COUNT; i++) {
                if (fabsf(stringTune[i] - 0.5f) >= TUNE_GAME_TOLERANCE) {
                    allInTune = false;
                    break;
                }
            }

            if (allInTune) {
                tuneTimer = 0.0f; // ideal tuning -> bar back to 100%
                showTuneGame = false;
                tuneCooldownTimer = TUNE_COOLDOWN_DURATION;
            } else if (tuneGameTimer <= 0.0f) {
                float totalCloseness = 0.0f;
                for (int i = 0; i < STRING_COUNT; i++) {
                    float closeness = 1.0f - fabsf(stringTune[i] - 0.5f) / 0.5f;
                    if (closeness < 0.0f) closeness = 0.0f;
                    totalCloseness += closeness;
                }
                float score = (totalCloseness / STRING_COUNT) * 100.0f;
                if (score <= 0.0f) tuneTimer = TUNE_DURATION_SECONDS;
                else if (score >= 100.0f) tuneTimer = 0.0f;
                else tuneTimer = -logf(score / 100.0f) / TUNE_DECAY_RATE;
                showTuneGame = false;
                tuneCooldownTimer = TUNE_COOLDOWN_DURATION;
            }
        } else if (gameOver) {
            Vector2 mouseVirtual = {GetMousePosition().x / SCALE, GetMousePosition().y / SCALE};
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouseVirtual, retryButtonRect)) {
                    resetGame();
                } else if (CheckCollisionPointRec(mouseVirtual, closeButtonRect)) {
                    requestClose = true;
                }
            }
        } else {
            auto handleEnemyDeath = [&](EnemyType typeBeforeHit, Vector2 deathCenter, bool died) {
                if (!died) return;

                PlaySound(enemyDeathSound);

                int soulReward = EnemySoulValue(typeBeforeHit);
                soulScore += soulReward;
                soulTextTimer = FLOATING_TEXT_DURATION;
                soulTextPosition = {deathCenter.x, deathCenter.y - 14.0f};
                soulTextAmount = soulReward;

                if (typeBeforeHit == EnemyType::SLIME) {
                    printf("SLIME DROPPED BEER\n");
                    float popDir = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
                    beerDrop.position = deathCenter;
                    beerDrop.velocity = {popDir * BEER_POP_SPEED, -BEER_POP_SPEED * 1.5f};
                    beerDrop.active = true;
                }
            };

            PlayerUpdate(player, dt, groundY, (float)VIRTUAL_WIDTH);

            Rectangle playerRect = {player.position.x, player.position.y, player.width, player.height};

            for (int ei = 0; ei < ENEMY_COUNT; ei++) {
                EnemyType typeBeforeUpdate = enemies[ei].type;
                Vector2 enemyCenterBeforeUpdate = {enemies[ei].position.x + enemies[ei].width / 2.0f, enemies[ei].position.y + enemies[ei].height / 2.0f};
                bool diedFromBurn = EnemyUpdate(enemies[ei], dt, groundY, (float)VIRTUAL_WIDTH, player.position.x + player.width / 2.0f);
                handleEnemyDeath(typeBeforeUpdate, enemyCenterBeforeUpdate, diedFromBurn);

                if (!hostilityDisabled && EnemyTryAttackPlayer(enemies[ei], playerRect)) {
                    player.health -= EnemyAttackDamage(enemies[ei].type);
                    PlaySound(playerHitSound);
                    if (player.health <= 0) {
                        player.health = 0;
                        gameOver = true;
                        PlaySound(playerDeathSound);
                    }
                }
            }

            if (player.slamImpactThisFrame) {
                Vector2 slamCenter = {player.position.x + player.width / 2.0f, player.position.y + player.height};
                for (int ei = 0; ei < ENEMY_COUNT; ei++) {
                    Vector2 enemyCenterBeforeSlam = {enemies[ei].position.x + enemies[ei].width / 2.0f, enemies[ei].position.y + enemies[ei].height / 2.0f};
                    float dx = enemyCenterBeforeSlam.x - slamCenter.x;
                    float dy = enemyCenterBeforeSlam.y - slamCenter.y;
                    if (sqrtf(dx * dx + dy * dy) <= SLAM_RADIUS) {
                        int scaledSlamDamage = (int)(SLAM_DAMAGE * (tuneValue / 100.0f));
                        EnemyType typeBeforeSlam = enemies[ei].type;
                        bool diedFromSlam = EnemyTakeDamage(enemies[ei], scaledSlamDamage, groundY, (float)VIRTUAL_WIDTH, true);
                        PlaySound(enemyHitSound);
                        handleEnemyDeath(typeBeforeSlam, enemyCenterBeforeSlam, diedFromSlam);
                    }
                }
            }

            if (player.justJumped) {
                PlaySound(jumpSound);
            }

            if (player.isAttacking && !player.attackHasHit) {
                Rectangle attackHitbox = PlayerGetAttackHitbox(player);
                for (int ei = 0; ei < ENEMY_COUNT; ei++) {
                    Rectangle enemyRect = {enemies[ei].position.x, enemies[ei].position.y, enemies[ei].width, enemies[ei].height};
                    if (CheckCollisionRecs(attackHitbox, enemyRect)) {
                        int scaledMeleeDamage = (int)(MELEE_DAMAGE * (tuneValue / 100.0f));
                        EnemyType typeBeforeMelee = enemies[ei].type;
                        Vector2 enemyCenterBeforeMelee = {enemies[ei].position.x + enemies[ei].width / 2.0f, enemies[ei].position.y + enemies[ei].height / 2.0f};
                        bool diedFromMelee = EnemyTakeDamage(enemies[ei], scaledMeleeDamage, groundY, (float)VIRTUAL_WIDTH);
                        PlaySound(enemyHitSound);
                        handleEnemyDeath(typeBeforeMelee, enemyCenterBeforeMelee, diedFromMelee);
                        player.attackHasHit = true;
                        break;
                    }
                }
            }

            if (tuneCooldownTimer > 0.0f) {
                tuneCooldownTimer -= dt;
                if (tuneCooldownTimer < 0.0f) tuneCooldownTimer = 0.0f;
            }

            auto startTuneGame = [&]() {
                if (tuneCooldownTimer > 0.0f) return;
                showSpellInfo = false;
                showSettings = false;
                showTuneGame = true;
                tuneGameTimer = TUNE_GAME_DURATION;
                lastCountdownSecond = -1;
                countdownPopTimer = 0.0f;
                selectedString = 0;
                tuneHoldTime = 0.0f;
                tuneHeldDirection = 0;
                for (int i = 0; i < STRING_COUNT; i++) {
                    stringTune[i] = GetRandomValue(10, 90) / 100.0f;
                }
            };

            if (IsKeyPressed(KEY_T)) {
                startTuneGame();
            }

            Vector2 mouseVirtual = {GetMousePosition().x / SCALE, GetMousePosition().y / SCALE};
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (showSpellInfo && CheckCollisionPointRec(mouseVirtual, infoCloseBtn)) {
                    showSpellInfo = false;
                } else if (showSettings && CheckCollisionPointRec(mouseVirtual, settingsCloseBtn)) {
                    showSettings = false;
                } else if (showSettings && CheckCollisionPointRec(mouseVirtual, hostilityToggleRect)) {
                    hostilityDisabled = !hostilityDisabled;
                } else if (CheckCollisionPointRec(mouseVirtual, scrollButtonRect)) {
                    showSpellInfo = !showSpellInfo;
                    showSettings = false;
                } else if (CheckCollisionPointRec(mouseVirtual, tuneButtonRect)) {
                    startTuneGame();
                } else if (CheckCollisionPointRec(mouseVirtual, settingsButtonRect)) {
                    showSettings = !showSettings;
                    showSpellInfo = false;
                }
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                showSettings = false;
                showSpellInfo = false;
            }

            auto applyCast = [&](Spell cast) {
                if (cast == Spell::FIREBALL) {
                    printf("FIREBALL CAST\n");
                    player.flashTimer = 0.3f;
                    PlayerStartCast(player, CastAnim::FIREBALL);

                    fireball.facingRight = player.facingRight;
                    fireball.position = {
                        player.position.x + player.width / 2,
                        player.position.y + player.height * 0.4f
                    };
                    fireball.active = true;
                    fireball.lifetime = FIREBALL_LIFETIME;

                    lastCastSpell = cast;
                    spellNameTimer = SPELL_NAME_DISPLAY_TIME;
                } else if (cast == Spell::LIGHT) {
                    printf("LIGHT CAST\n");
                    lightActive = true;
                    lightTimer = LIGHT_DURATION;

                    lastCastSpell = cast;
                    spellNameTimer = SPELL_NAME_DISPLAY_TIME;
                } else if (cast == Spell::DASH) {
                    printf("DASH CAST\n");
                    PlayerStartDash(player);

                    lastCastSpell = cast;
                    spellNameTimer = SPELL_NAME_DISPLAY_TIME;
                } else if (cast == Spell::FROST) {
                    printf("FROST CAST\n");
                    PlayerStartCast(player, CastAnim::FROST);
                    Vector2 blastCenter = {
                        player.position.x + player.width / 2,
                        player.position.y + player.height / 2
                    };
                    frostBlast.position = blastCenter;
                    frostBlast.active = true;
                    frostBlast.timer = FROST_BLAST_ANIM_DURATION;

                    for (int ei = 0; ei < ENEMY_COUNT; ei++) {
                        Vector2 enemyCenterBeforeFrost = {enemies[ei].position.x + enemies[ei].width / 2, enemies[ei].position.y + enemies[ei].height / 2};
                        float dx = enemyCenterBeforeFrost.x - blastCenter.x;
                        float dy = enemyCenterBeforeFrost.y - blastCenter.y;
                        if (sqrtf(dx * dx + dy * dy) <= FROST_BLAST_RADIUS) {
                            int scaledFrostDamage = (int)(FROST_BLAST_DAMAGE * (tuneValue / 100.0f));
                            EnemyType typeBeforeFrost = enemies[ei].type;
                            bool diedFromFrost = EnemyTakeDamage(enemies[ei], scaledFrostDamage, groundY, (float)VIRTUAL_WIDTH);
                            PlaySound(enemyHitSound);
                            handleEnemyDeath(typeBeforeFrost, enemyCenterBeforeFrost, diedFromFrost);
                            if (enemies[ei].health > 0) {
                                EnemyKnockback(enemies[ei], blastCenter, FROST_BLAST_KNOCKBACK_FORCE, (float)VIRTUAL_WIDTH);
                            }
                        }
                    }

                    lastCastSpell = cast;
                    spellNameTimer = SPELL_NAME_DISPLAY_TIME;
                }
            };

            Note pressedNotes[NOTE_KEY_COUNT];
            int pressedCount = GetNotesPressed(pressedNotes, NOTE_KEY_COUNT);
            for (int n = 0; n < pressedCount; n++) {
                Note note = pressedNotes[n];
                printf("Note: %s\n", NoteToString(note));

                for (int i = 0; i < NOTE_KEY_COUNT; i++) {
                    if (bindings[i].note == note) {
                        PlaySound(noteSounds[i]);
                        break;
                    }
                }

                applyCast(ChordBurstAddNote(chordBurst, note));
            }

#if defined(PLATFORM_IOS)
            Note touchNotes[CHORD_WHEEL_GATE_COUNT];
            int touchNoteCount = TouchControlsNotesPressed(touchNotes, CHORD_WHEEL_GATE_COUNT);
            for (int n = 0; n < touchNoteCount; n++) {
                Note note = touchNotes[n];
                printf("Note: %s\n", NoteToString(note));

                for (int i = 0; i < NOTE_KEY_COUNT; i++) {
                    if (bindings[i].note == note) {
                        PlaySound(noteSounds[i]);
                        break;
                    }
                }

                applyCast(ChordBurstAddNote(chordBurst, note));
            }
#endif

            applyCast(ChordBurstUpdate(chordBurst));

            if (lightActive) {
                lightTimer -= dt;
                if (lightTimer <= 0.0f) {
                    lightActive = false;
                    lightTimer = 0.0f;
                }
            }

            tuneTimer += dt;

            if (spellNameTimer > 0.0f) {
                spellNameTimer -= dt;
            }

            if (fireball.active) {
                fireball.position.x += (fireball.facingRight ? FIREBALL_SPEED : -FIREBALL_SPEED) * dt;
                fireball.lifetime -= dt;
                if (fireball.lifetime <= 0.0f || fireball.position.x < 0 || fireball.position.x > VIRTUAL_WIDTH) {
                    fireball.active = false;
                }

                for (int ei = 0; ei < ENEMY_COUNT && fireball.active; ei++) {
                    Rectangle enemyRect = {enemies[ei].position.x, enemies[ei].position.y, enemies[ei].width, enemies[ei].height};
                    if (CheckCollisionCircleRec(fireball.position, FIREBALL_RADIUS, enemyRect)) {
                        int scaledFireballDamage = (int)(FIREBALL_DAMAGE * (tuneValue / 100.0f));
                        EnemyType typeBeforeFireball = enemies[ei].type;
                        Vector2 enemyCenterBeforeFireball = {enemies[ei].position.x + enemies[ei].width / 2.0f, enemies[ei].position.y + enemies[ei].height / 2.0f};
                        bool diedFromFireball = EnemyTakeDamage(enemies[ei], scaledFireballDamage, groundY, (float)VIRTUAL_WIDTH);
                        PlaySound(enemyHitSound);
                        handleEnemyDeath(typeBeforeFireball, enemyCenterBeforeFireball, diedFromFireball);
                        if (enemies[ei].health > 0 && enemies[ei].type != EnemyType::SPIDER) {
                            int scaledBurnDamage = (int)(FIREBALL_BURN_DAMAGE * (tuneValue / 100.0f));
                            EnemyIgnite(enemies[ei], scaledBurnDamage, FIREBALL_BURN_DURATION);
                        }
                        fireball.active = false;
                    }
                }
            }

            if (frostBlast.active) {
                frostBlast.timer -= dt;
                if (frostBlast.timer <= 0.0f) {
                    frostBlast.active = false;
                }
            }

            if (beerDrop.active) {
                float beerHalfHeight = BEER_DROP_SIZE * 0.6f; // DrawBeerIcon's h/2, h = size * 1.2
                bool onGround = (beerDrop.position.y + beerHalfHeight >= groundY);
                if (!onGround) {
                    beerDrop.velocity.y += BEER_POP_GRAVITY * dt;
                    beerDrop.position.x += beerDrop.velocity.x * dt;
                    beerDrop.position.y += beerDrop.velocity.y * dt;
                    if (beerDrop.position.y + beerHalfHeight >= groundY) {
                        beerDrop.position.y = groundY - beerHalfHeight;
                        beerDrop.velocity = {0, 0};
                        beerDrop.groundTimer = BEER_GROUND_DURATION;
                    }
                } else {
                    beerDrop.groundTimer -= dt;
                    if (beerDrop.groundTimer <= 0.0f) {
                        beerDrop.active = false;
                    }
                }
            }

            if (beerDrop.active) {
                float beerHalfHeight = BEER_DROP_SIZE * 0.6f;
                Rectangle beerRect = {
                    beerDrop.position.x - BEER_DROP_SIZE / 2.0f,
                    beerDrop.position.y - beerHalfHeight,
                    BEER_DROP_SIZE,
                    beerHalfHeight * 2.0f
                };
                if (CheckCollisionRecs(beerRect, playerRect)) {
                    beerDrop.active = false;
                    PlayerDrinkBeer(player);
                    healTextTimer = FLOATING_TEXT_DURATION;
                    bloatTextTimer = FLOATING_TEXT_DURATION;
                }
            }

            if (healTextTimer > 0.0f) {
                healTextTimer -= dt;
                if (healTextTimer < 0.0f) healTextTimer = 0.0f;
            }
            if (bloatTextTimer > 0.0f) {
                bloatTextTimer -= dt;
                if (bloatTextTimer < 0.0f) bloatTextTimer = 0.0f;
            }
            if (soulTextTimer > 0.0f) {
                soulTextTimer -= dt;
                if (soulTextTimer < 0.0f) soulTextTimer = 0.0f;
            }
        }

#if defined(PLATFORM_IOS)
        BeginDrawing();
        BeginMode2D(camera);
#else
        BeginTextureMode(canvas);
#endif
            ClearBackground(BLACK);
            if (showTitleScreen) {
                DrawTexturePro(caveTexture, caveSrc, caveDst, {0, 0}, 0.0f, WHITE);
                DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, Fade(BLACK, 0.45f));

                float bob = sinf(titleBobTimer * 2.0f) * 5.0f;
                const char *titleText = "MAGIC PICK";
                int titleFontSize = 28;
                int titleWidth = MeasureText(titleText, titleFontSize);
                int titleX = (VIRTUAL_WIDTH - titleWidth) / 2;
                int titleY = (int)(46 + bob);

                // Small gold pick icon and twinkling sparkles for a
                // "magical" feel, since it's a guitar-pick pun on the title.
                float pickX = titleX - 14;
                float pickY = titleY + titleFontSize * 0.5f;
                DrawTriangle({pickX, pickY - 6}, {pickX - 5, pickY + 5}, {pickX + 5, pickY + 5}, GOLD);
                DrawTriangleLines({pickX, pickY - 6}, {pickX - 5, pickY + 5}, {pickX + 5, pickY + 5}, Color{120, 90, 10, 255});

                for (int s = 0; s < 4; s++) {
                    float angle = titleBobTimer * 1.5f + s * (PI / 2.0f);
                    float sparkleX = titleX + titleWidth * (0.15f + 0.7f * (s / 3.0f));
                    float sparkleY = titleY - 10.0f + sinf(angle) * 6.0f;
                    float sparkleAlpha = 0.4f + 0.6f * (sinf(titleBobTimer * 3.0f + s) * 0.5f + 0.5f);
                    Color sparkleColor = Fade(Color{255, 240, 180, 255}, sparkleAlpha);
                    DrawLineEx({sparkleX - 2, sparkleY}, {sparkleX + 2, sparkleY}, 1.0f, sparkleColor);
                    DrawLineEx({sparkleX, sparkleY - 2}, {sparkleX, sparkleY + 2}, 1.0f, sparkleColor);
                }

                DrawText(titleText, titleX + 1, titleY + 1, titleFontSize, Fade(BLACK, 0.6f));
                DrawText(titleText, titleX, titleY, titleFontSize, GOLD);

                DrawRectangleRec(titleStartButtonRect, RAYWHITE);
                DrawRectangleLinesEx(titleStartButtonRect, 2.0f, BLACK);
                const char *startText = "START";
                int startWidth = MeasureText(startText, 10);
                DrawText(startText, (int)(titleStartButtonRect.x + (titleStartButtonRect.width - startWidth) / 2), (int)titleStartButtonRect.y + 4, 10, BLACK);

                DrawRectangleRec(titleCloseButtonRect, RAYWHITE);
                DrawRectangleLinesEx(titleCloseButtonRect, 2.0f, BLACK);
                const char *titleCloseText = "CLOSE";
                int titleCloseWidth = MeasureText(titleCloseText, 10);
                DrawText(titleCloseText, (int)(titleCloseButtonRect.x + (titleCloseButtonRect.width - titleCloseWidth) / 2), (int)titleCloseButtonRect.y + 4, 10, BLACK);
            } else {
            DrawTexturePro(caveTexture, caveSrc, caveDst, {0, 0}, 0.0f, WHITE);
            DrawLine(0, (int)groundY, VIRTUAL_WIDTH, (int)groundY, DARKGRAY);
            PlayerDraw(player, playerSprites);
            for (int ei = 0; ei < ENEMY_COUNT; ei++) {
                EnemyDraw(enemies[ei]);
            }

            if (fireball.active) {
                Rectangle fireballSrc = {0, 0, (float)fireballTexture.width, (float)fireballTexture.height};
                if (!fireball.facingRight) fireballSrc.width = -fireballSrc.width;
                Rectangle fireballDst = {fireball.position.x, fireball.position.y, FIREBALL_SPRITE_WIDTH, FIREBALL_SPRITE_HEIGHT};
                Vector2 fireballOrigin = {FIREBALL_SPRITE_WIDTH / 2.0f, FIREBALL_SPRITE_HEIGHT / 2.0f};
                DrawTexturePro(fireballTexture, fireballSrc, fireballDst, fireballOrigin, 0.0f, WHITE);
            }

            if (frostBlast.active) {
                float frostProgress = 1.0f - (frostBlast.timer / FROST_BLAST_ANIM_DURATION);
                DrawFrostBlast(frostBlast.position, FROST_BLAST_RADIUS, frostProgress);
            }

            if (beerDrop.active) {
                DrawBeerIcon(beerDrop.position, BEER_DROP_SIZE, 1.0f);
            }

            // Floating pickup feedback: heal text rises above bloat text so
            // the two never overlap, even though both start at once.
            if (healTextTimer > 0.0f) {
                float t = 1.0f - (healTextTimer / FLOATING_TEXT_DURATION);
                float riseY = t * 16.0f;
                float alpha = 1.0f - t;
                const char *healText = "+20 HEALTH";
                int healTextWidth = MeasureText(healText, 8);
                float healX = player.position.x + player.width / 2.0f - healTextWidth / 2.0f;
                float healY = player.position.y - 22.0f - riseY;
                DrawText(healText, (int)healX, (int)healY, 8, Fade(GREEN, alpha));
            }
            if (bloatTextTimer > 0.0f) {
                float t = 1.0f - (bloatTextTimer / FLOATING_TEXT_DURATION);
                float riseY = t * 16.0f;
                float alpha = 1.0f - t;
                const char *bloatText = "BLOATED";
                int bloatTextWidth = MeasureText(bloatText, 8);
                float bloatX = player.position.x + player.width / 2.0f - bloatTextWidth / 2.0f;
                float bloatY = player.position.y - 10.0f - riseY;
                DrawText(bloatText, (int)bloatX, (int)bloatY, 8, Fade(SKYBLUE, alpha));
            }

            if (soulTextTimer > 0.0f) {
                float t = 1.0f - (soulTextTimer / FLOATING_TEXT_DURATION);
                float riseY = t * 16.0f;
                float alpha = 1.0f - t;
                char soulText[24];
                snprintf(soulText, sizeof(soulText), "+%d Souls", soulTextAmount);
                int soulTextWidth = MeasureText(soulText, 8);
                float soulX = soulTextPosition.x - soulTextWidth / 2.0f;
                float soulY = soulTextPosition.y - riseY;
                DrawText(soulText, (int)soulX, (int)soulY, 8, Fade(YELLOW, alpha));
            }

            if (!lightActive) {
                DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, Fade(BLACK, AMBIENT_DARK_ALPHA));
            } else {
                float lightStrength = (lightTimer > LIGHT_FADE_DURATION)
                    ? 1.0f
                    : (lightTimer / LIGHT_FADE_DURATION);

                // Pulses the light's brightness between 100% and LIGHT_PULSE_MIN so it
                // reads as actively illuminating rather than a static hole in the dark.
                float elapsedSinceCast = LIGHT_DURATION - lightTimer;
                float pulseMid = (1.0f + LIGHT_PULSE_MIN) / 2.0f;
                float pulseAmplitude = (1.0f - LIGHT_PULSE_MIN) / 2.0f;
                float pulse = pulseMid + pulseAmplitude * sinf(elapsedSinceCast * (2.0f * PI / LIGHT_PULSE_PERIOD));

                float centerDarkAlpha = AMBIENT_DARK_ALPHA * (1.0f - lightStrength * pulse);

                Vector2 lightCenter = {
                    player.position.x + player.width / 2,
                    player.position.y + player.height / 2
                };

                for (int y = 0; y < VIRTUAL_HEIGHT; y++) {
                    for (int x = 0; x < VIRTUAL_WIDTH; x++) {
                        float dx = x - lightCenter.x;
                        float dy = y - lightCenter.y;
                        float t = sqrtf(dx * dx + dy * dy) / LIGHT_RADIUS;
                        if (t > 1.0f) t = 1.0f;
                        float darkAlpha = centerDarkAlpha + (AMBIENT_DARK_ALPHA - centerDarkAlpha) * t;
                        lightMaskPixels[y * VIRTUAL_WIDTH + x] = Color{0, 0, 0, (unsigned char)(darkAlpha * 255)};
                    }
                }
                UpdateTexture(lightMaskTexture, lightMaskPixels.data());
                DrawTexture(lightMaskTexture, 0, 0, WHITE);
            }

            if (frostBlast.active) {
                // A light blue glow marking the blast radius, drawn over the darkness
                // so it reads clearly even in a dark cave. Brightness ramps up with
                // the burst instead of appearing instantly at full strength.
                float frostProgress = 1.0f - (frostBlast.timer / FROST_BLAST_ANIM_DURATION);
                float glowAlpha = frostProgress * FROST_LIGHT_MAX_ALPHA;
                Color frostLightInner = Fade(Color{160, 220, 255, 255}, glowAlpha);
                Color frostLightOuter = Fade(Color{160, 220, 255, 255}, 0.0f);
                DrawCircleGradientCompat(frostBlast.position.x, frostBlast.position.y, FROST_BLAST_RADIUS, frostLightInner, frostLightOuter);
            }

            // HUD backdrops: indicators read the player's state, not the
            // environment, so they sit on their own panels immune to the
            // darkness overlay above. Each panel is sized to the actual
            // content plus a small margin, with only a trailing edge fading
            // out (dissipating) toward the middle of the screen.
            Color panelClear = Fade(RAYWHITE, 0.0f);

            int barLabelWidth = MeasureText("TUNE", BAR_LABEL_FONT_SIZE); // widest of "HP"/"TUNE"
            int leftContentWidth = 4 + (int)HEALTH_BAR_WIDTH + 3 + barLabelWidth + 4;
            DrawRectangle(0, 0, leftContentWidth, HUD_PANEL_HEIGHT, RAYWHITE);
            DrawRectangleGradientH(leftContentWidth, 0, HUD_PANEL_FADE_WIDTH, HUD_PANEL_HEIGHT, RAYWHITE, panelClear);

            int stanceTextWidthMax = MeasureText("STANCE: MAJOR", STANCE_FONT_SIZE); // same length as MINOR
            int rightContentWidth = stanceTextWidthMax + 8;
            int rightContentStart = VIRTUAL_WIDTH - rightContentWidth;
            DrawRectangleGradientH(rightContentStart - HUD_PANEL_FADE_WIDTH, 0, HUD_PANEL_FADE_WIDTH, HUD_PANEL_HEIGHT, panelClear, RAYWHITE);
            DrawRectangle(rightContentStart, 0, rightContentWidth, HUD_PANEL_HEIGHT, RAYWHITE);

            Rectangle healthBg = {4, 4, HEALTH_BAR_WIDTH, HEALTH_BAR_HEIGHT};
            float healthRatio = (float)player.health / PLAYER_MAX_HEALTH;
            Rectangle healthFg = {4, 4, HEALTH_BAR_WIDTH * healthRatio, HEALTH_BAR_HEIGHT};
            DrawRectangleRec(healthBg, DARKGRAY);
            DrawRectangleRec(healthFg, GREEN);
            DrawRectangleLinesEx(healthBg, 1.0f, BLACK);
            DrawText("HP", (int)(4 + HEALTH_BAR_WIDTH + 3), 4, BAR_LABEL_FONT_SIZE, BLACK);
            {
                char healthText[16];
                snprintf(healthText, sizeof(healthText), "%d/%d", player.health, PLAYER_MAX_HEALTH);
                int healthTextWidth = MeasureText(healthText, 6);
                DrawText(healthText, (int)(4 + (HEALTH_BAR_WIDTH - healthTextWidth) / 2), 5, 6, BLACK);
            }

            float tuneY = 4 + HEALTH_BAR_HEIGHT + TUNE_BAR_GAP;
            Rectangle tuneBg = {4, tuneY, HEALTH_BAR_WIDTH, TUNE_BAR_HEIGHT};
            Rectangle tuneFg = {4, tuneY, HEALTH_BAR_WIDTH * (tuneValue / 100.0f), TUNE_BAR_HEIGHT};
            DrawRectangleRec(tuneBg, DARKGRAY);
            DrawRectangleRec(tuneFg, SKYBLUE);
            DrawRectangleLinesEx(tuneBg, 1.0f, BLACK);
            DrawText("TUNE", (int)(4 + HEALTH_BAR_WIDTH + 3), (int)tuneY, BAR_LABEL_FONT_SIZE, BLACK);
            {
                char tuneText[16];
                snprintf(tuneText, sizeof(tuneText), "%d/100", (int)tuneValue);
                int tuneTextWidth = MeasureText(tuneText, 6);
                DrawText(tuneText, (int)(4 + (HEALTH_BAR_WIDTH - tuneTextWidth) / 2), (int)tuneY + 1, 6, BLACK);
            }

            const char *stanceText = StanceToString(player.stance);
            int textWidth = MeasureText(stanceText, STANCE_FONT_SIZE);
            DrawText(stanceText, VIRTUAL_WIDTH - textWidth - 4, 4, STANCE_FONT_SIZE, BLACK);

            if (spellNameTimer > 0.0f) {
                const char *spellText = SpellToString(lastCastSpell);
                int spellTextWidth = MeasureText(spellText, SPELL_NAME_FONT_SIZE);
                Color spellColor = ORANGE;
                if (lastCastSpell == Spell::LIGHT) spellColor = Color{184, 134, 11, 255}; // dark gold, readable on the light panel
                else if (lastCastSpell == Spell::DASH) spellColor = SKYBLUE;
                else if (lastCastSpell == Spell::FROST) spellColor = Color{70, 160, 220, 255};
                DrawText(spellText, VIRTUAL_WIDTH - spellTextWidth - 4, 14, SPELL_NAME_FONT_SIZE, spellColor);
            }

            // Soul score: ghost icon + running count, top-right below the stance panel.
            {
                char soulScoreText[24];
                snprintf(soulScoreText, sizeof(soulScoreText), "%d", soulScore);
                int soulScoreTextWidth = MeasureText(soulScoreText, 10);
                float soulsPanelWidth = 20.0f + soulScoreTextWidth + 8.0f;
                float soulsPanelHeight = 16.0f;
                Rectangle soulsPanelRect = {VIRTUAL_WIDTH - soulsPanelWidth - 4, HUD_PANEL_HEIGHT + 4, soulsPanelWidth, soulsPanelHeight};
                DrawRectangleRounded(soulsPanelRect, 0.3f, 4, RAYWHITE);
                DrawRectangleRoundedLinesEx(soulsPanelRect, 0.3f, 4, 1.0f, BLACK);
                DrawGhostIcon({soulsPanelRect.x + 9, soulsPanelRect.y + soulsPanelHeight / 2.0f}, 10.0f);
                DrawText(soulScoreText, (int)(soulsPanelRect.x + 18), (int)(soulsPanelRect.y + 4), 10, BLACK);
            }

            // Scroll button: click to toggle the spell reference panel.
            DrawRectangleRec(scrollButtonRect, PARCHMENT_COLOR);
            DrawRectangleLinesEx(scrollButtonRect, 1.0f, BLACK);
            DrawLine((int)scrollButtonRect.x + 2, (int)scrollButtonRect.y + 4, (int)scrollButtonRect.x + 14, (int)scrollButtonRect.y + 4, BROWN);
            DrawLine((int)scrollButtonRect.x + 2, (int)scrollButtonRect.y + 12, (int)scrollButtonRect.x + 14, (int)scrollButtonRect.y + 12, BROWN);
            DrawLine((int)scrollButtonRect.x + 3, (int)scrollButtonRect.y + 7, (int)scrollButtonRect.x + 13, (int)scrollButtonRect.y + 7, DARKBROWN);
            DrawLine((int)scrollButtonRect.x + 3, (int)scrollButtonRect.y + 9, (int)scrollButtonRect.x + 13, (int)scrollButtonRect.y + 9, DARKBROWN);

            // Tune button: click to toggle the string-tuning minigame.
            DrawRectangleRec(tuneButtonRect, LIGHTGRAY);
            DrawRectangleLinesEx(tuneButtonRect, 1.0f, BLACK);
            DrawCircle((int)tuneButtonRect.x + 8, (int)tuneButtonRect.y + 6, 4.0f, GRAY);
            DrawCircleLines((int)tuneButtonRect.x + 8, (int)tuneButtonRect.y + 6, 4.0f, BLACK);
            DrawRectangle((int)tuneButtonRect.x + 7, (int)tuneButtonRect.y + 9, 2, 5, DARKGRAY);
            if (tuneCooldownTimer > 0.0f) {
                DrawRectangleRec(tuneButtonRect, Fade(BLACK, 0.55f));
                char cooldownText[8];
                snprintf(cooldownText, sizeof(cooldownText), "%.0f", tuneCooldownTimer);
                int cooldownWidth = MeasureText(cooldownText, 8);
                DrawText(cooldownText, (int)(tuneButtonRect.x + (tuneButtonRect.width - cooldownWidth) / 2), (int)tuneButtonRect.y + 4, 8, WHITE);
            }

            // Settings button: click to toggle the keybindings panel.
            DrawRectangleRec(settingsButtonRect, LIGHTGRAY);
            DrawRectangleLinesEx(settingsButtonRect, 1.0f, BLACK);
            {
                float gx = settingsButtonRect.x + settingsButtonRect.width / 2;
                float gy = settingsButtonRect.y + settingsButtonRect.height / 2;
                for (int t = 0; t < 8; t++) {
                    float angle = t * (PI / 4.0f);
                    DrawRectangle((int)(gx + cosf(angle) * 5.0f) - 1, (int)(gy + sinf(angle) * 5.0f) - 1, 2, 2, DARKGRAY);
                }
                DrawCircle((int)gx, (int)gy, 4.0f, GRAY);
                DrawCircle((int)gx, (int)gy, 1.5f, DARKGRAY);
            }

            if (showTuneGame) {
                Color inkColor = {58, 41, 26, 255};
                DrawRectangleRounded(tuneGameRect, 0.06f, 8, RAYWHITE);
                DrawRectangleRoundedLinesEx(tuneGameRect, 0.06f, 8, 2.0f, inkColor);

                const char *tuneTitle = "TUNING";
                int tuneTitleWidth = MeasureText(tuneTitle, 12);
                DrawText(tuneTitle, (int)(tuneGameRect.x + (tuneGameRect.width - tuneTitleWidth) / 2), (int)tuneGameRect.y + 4, 12, inkColor);

                float countdownPop = countdownPopTimer / COUNTDOWN_POP_DURATION;
                int countdownFontSize = 16 + (int)(10.0f * countdownPop);
                char timeText[8];
                snprintf(timeText, sizeof(timeText), "%d", lastCountdownSecond);
                int timeTextWidth = MeasureText(timeText, countdownFontSize);
                int timeTextX = (int)(tuneGameRect.x + tuneGameRect.width - timeTextWidth - 8);
                int timeTextY = (int)tuneGameRect.y + 2;
                DrawText(timeText, timeTextX + 1, timeTextY + 1, countdownFontSize, Fade(BLACK, 0.5f));
                DrawText(timeText, timeTextX, timeTextY, countdownFontSize, RED);

                for (int i = 0; i < STRING_COUNT; i++) {
                    Rectangle track = stringTrackRect[i];
                    float rowY = tuneGameRect.y + 20 + i * 16;

                    if (i == selectedString) {
                        DrawRectangle((int)tuneGameRect.x + 4, (int)rowY - 2, (int)tuneGameRect.width - 8, 14, Fade(YELLOW, 0.4f));
                    }
                    DrawText(STRING_LABELS[i], (int)tuneGameRect.x + 10, (int)rowY, 8, inkColor);
                    DrawRectangleRec(track, DARKGRAY);

                    Rectangle targetZone = {
                        track.x + track.width * 0.45f, track.y - 1, track.width * 0.1f, track.height + 2
                    };
                    DrawRectangleRec(targetZone, LIME);

                    bool inTune = fabsf(stringTune[i] - 0.5f) < TUNE_GAME_TOLERANCE;
                    float indicatorX = track.x + stringTune[i] * track.width;
                    DrawRectangle((int)indicatorX - 2, (int)track.y - 4, 4, (int)track.height + 8, inTune ? GREEN : RED);
                }

                const char *hint = "ARROWS: SELECT/TUNE  T: CLOSE";
                int hintWidth = MeasureText(hint, 7);
                DrawText(hint, (int)(tuneGameRect.x + (tuneGameRect.width - hintWidth) / 2), (int)(tuneGameRect.y + tuneGameRect.height - 12), 7, inkColor);
            }

            float pianoTotalWidth = NOTE_KEY_COUNT * PIANO_KEY_WIDTH + (NOTE_KEY_COUNT - 1) * PIANO_KEY_GAP;
            float pianoX = (VIRTUAL_WIDTH - pianoTotalWidth) / 2.0f;
            float pianoY = groundY + 3.0f;
            DrawTexture(pianoPanelMask, (int)pianoX - 2, (int)pianoY - 2, WHITE);
            for (int i = 0; i < NOTE_KEY_COUNT; i++) {
                float keyX = pianoX + i * (PIANO_KEY_WIDTH + PIANO_KEY_GAP);
                Rectangle keyRect = {keyX, pianoY, PIANO_KEY_WIDTH, PIANO_KEY_HEIGHT};
                bool isDown = IsKeyDown(bindings[i].key);
                DrawRectangleRec(keyRect, isDown ? YELLOW : RAYWHITE);
                DrawRectangleLinesEx(keyRect, 1.0f, BLACK);
                int labelWidth = MeasureText(bindings[i].label, 10);
                DrawText(bindings[i].label, (int)(keyX + (PIANO_KEY_WIDTH - labelWidth) / 2), (int)(pianoY + 2), 10, BLACK);
            }

            if (showSpellInfo) {
                Color inkColor = {58, 41, 26, 255}; // dark ink brown, reads well on parchment

                // Parchment scroll: rounded body, rolled bar top/bottom, ink-brown border.
                DrawRectangleRounded(infoRect, 0.08f, 8, PARCHMENT_COLOR);
                DrawRectangleRoundedLinesEx(infoRect, 0.08f, 8, 2.0f, inkColor);
                DrawRectangle((int)infoRect.x + 4, (int)infoRect.y + 2, (int)infoRect.width - 8, 4, BROWN);
                DrawRectangle((int)infoRect.x + 4, (int)infoRect.y + infoRect.height - 6, (int)infoRect.width - 8, 4, BROWN);

                const char *title = "SPELLS";
                int titleWidth = MeasureText(title, 12);
                DrawText(title, (int)(infoRect.x + (infoRect.width - titleWidth) / 2), (int)infoRect.y + 10, 12, MAROON);

                DrawRectangleRec(infoCloseBtn, RAYWHITE);
                DrawRectangleLinesEx(infoCloseBtn, 1.0f, inkColor);
                DrawLine((int)infoCloseBtn.x + 3, (int)infoCloseBtn.y + 3, (int)infoCloseBtn.x + 9, (int)infoCloseBtn.y + 9, inkColor);
                DrawLine((int)infoCloseBtn.x + 9, (int)infoCloseBtn.y + 3, (int)infoCloseBtn.x + 3, (int)infoCloseBtn.y + 9, inkColor);

                int col1X = (int)infoRect.x + 10;
                int col2X = (int)infoRect.x + 70;
                int col3X = (int)infoRect.x + 160;
                int headerY = (int)infoRect.y + 26;
                DrawText("SPELL", col1X, headerY, 8, inkColor);
                DrawText("NOTES", col2X, headerY, 8, inkColor);
                DrawText("KEYS", col3X, headerY, 8, inkColor);
                DrawLine((int)infoRect.x + 6, headerY + 10, (int)(infoRect.x + infoRect.width - 6), headerY + 10, inkColor);

                int lineY = headerY + 16;
                int chordCount = GetChordDefinitionCount();
                for (int i = 0; i < chordCount; i++) {
                    Spell spell;
                    const Note *notes;
                    int length;
                    GetChordDefinition(i, spell, notes, length);

                    char notesText[32];
                    char keysText[32];
                    int notesPos = 0;
                    int keysPos = 0;
                    for (int n = 0; n < length; n++) {
                        char keyChar = '?';
                        for (int k = 0; k < NOTE_KEY_COUNT; k++) {
                            if (bindings[k].note == notes[n]) {
                                keyChar = (char)bindings[k].key;
                                break;
                            }
                        }
                        const char *sep = (n < length - 1) ? ", " : "";
                        notesPos += snprintf(notesText + notesPos, sizeof(notesText) - notesPos, "%s%s", NoteToString(notes[n]), sep);
                        keysPos += snprintf(keysText + keysPos, sizeof(keysText) - keysPos, "%c%s", keyChar, sep);
                    }

                    DrawText(SpellToString(spell), col1X, lineY, 8, inkColor);
                    DrawText(notesText, col2X, lineY, 8, inkColor);
                    DrawText(keysText, col3X, lineY, 8, inkColor);
                    lineY += 14;
                }
            }

            if (showSettings) {
                DrawRectangleRounded(settingsRect, 0.06f, 8, RAYWHITE);
                DrawRectangleRoundedLinesEx(settingsRect, 0.06f, 8, 2.0f, BLACK);

                const char *settingsTitle = "SETTINGS";
                int settingsTitleWidth = MeasureText(settingsTitle, 12);
                DrawText(settingsTitle, (int)(settingsRect.x + (settingsRect.width - settingsTitleWidth) / 2), (int)settingsRect.y + 4, 12, BLACK);

                DrawRectangleRec(settingsCloseBtn, RAYWHITE);
                DrawRectangleLinesEx(settingsCloseBtn, 1.0f, BLACK);
                DrawLine((int)settingsCloseBtn.x + 3, (int)settingsCloseBtn.y + 3, (int)settingsCloseBtn.x + 9, (int)settingsCloseBtn.y + 9, BLACK);
                DrawLine((int)settingsCloseBtn.x + 9, (int)settingsCloseBtn.y + 3, (int)settingsCloseBtn.x + 3, (int)settingsCloseBtn.y + 9, BLACK);

                struct KeyBindingRow {
                    const char *action;
                    const char *keys;
                };
                static const KeyBindingRow rows[] = {
                    {"Move", "LEFT / RIGHT"},
                    {"Jump", "UP"},
                    {"Attack", "SPACE"},
                    {"Stance Major / Minor", "1 / 2"},
                    {"Notes C D E F G A B", "Q A W S E D R"},
                    {"Tune minigame", "T (10s cooldown)"},
                    {"Spells / Tune / Settings", "click the icons"},
                    {"Close Spells / Settings", "ESC or X button"},
                };
                int rowCount = sizeof(rows) / sizeof(rows[0]);

                int rowY = (int)settingsRect.y + 20;
                for (int i = 0; i < rowCount; i++) {
                    DrawText(rows[i].action, (int)settingsRect.x + 10, rowY, 8, BLACK);
                    int keysWidth = MeasureText(rows[i].keys, 8);
                    DrawText(rows[i].keys, (int)(settingsRect.x + settingsRect.width - keysWidth - 10), rowY, 8, DARKGRAY);
                    rowY += 13;
                }

                DrawLine((int)settingsRect.x + 10, (int)settingsRect.y + 123, (int)(settingsRect.x + settingsRect.width - 10), (int)settingsRect.y + 123, LIGHTGRAY);

                Color hostilityBtnColor = hostilityDisabled ? Color{200, 90, 90, 255} : LIGHTGRAY;
                DrawRectangleRec(hostilityToggleRect, hostilityBtnColor);
                DrawRectangleLinesEx(hostilityToggleRect, 1.0f, BLACK);
                const char *hostilityText = hostilityDisabled ? "Disable Hostility: ON" : "Disable Hostility: OFF";
                int hostilityTextWidth = MeasureText(hostilityText, 8);
                DrawText(hostilityText,
                    (int)(hostilityToggleRect.x + (hostilityToggleRect.width - hostilityTextWidth) / 2),
                    (int)(hostilityToggleRect.y + (hostilityToggleRect.height - 8) / 2),
                    8, BLACK);
            }

            if (gameOver) {
                DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, Fade(BLACK, 0.75f));

                const char *deadText = "YOU DIED";
                int deadWidth = MeasureText(deadText, 24);
                DrawText(deadText, (VIRTUAL_WIDTH - deadWidth) / 2, VIRTUAL_HEIGHT / 2 - 40, 24, RED);

                char soulsCollectedText[32];
                snprintf(soulsCollectedText, sizeof(soulsCollectedText), "Souls Collected: %d", soulScore);
                int soulsCollectedWidth = MeasureText(soulsCollectedText, 10);
                DrawText(soulsCollectedText, (VIRTUAL_WIDTH - soulsCollectedWidth) / 2, VIRTUAL_HEIGHT / 2 - 8, 10, YELLOW);

                DrawRectangleRec(retryButtonRect, RAYWHITE);
                DrawRectangleLinesEx(retryButtonRect, 1.0f, BLACK);
                const char *retryText = "RETRY";
                int retryWidth = MeasureText(retryText, 10);
                DrawText(retryText, (int)(retryButtonRect.x + (retryButtonRect.width - retryWidth) / 2), (int)retryButtonRect.y + 4, 10, BLACK);

                DrawRectangleRec(closeButtonRect, RAYWHITE);
                DrawRectangleLinesEx(closeButtonRect, 1.0f, BLACK);
                const char *closeText = "CLOSE";
                int closeWidth = MeasureText(closeText, 10);
                DrawText(closeText, (int)(closeButtonRect.x + (closeButtonRect.width - closeWidth) / 2), (int)closeButtonRect.y + 4, 10, BLACK);
            }
            }
#if defined(PLATFORM_IOS)
        if (!showTitleScreen) {
            TouchControlsDraw();
        }
        EndMode2D();
        EndDrawing();
#else
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(canvas.texture, canvasSrc, canvasDst, {0, 0}, 0.0f, WHITE);
        EndDrawing();
#endif
    }

    for (int i = 0; i < NOTE_KEY_COUNT; i++) {
        UnloadSound(noteSounds[i]);
    }
    UnloadSound(jumpSound);
    UnloadSound(enemyDeathSound);
    UnloadSound(enemyHitSound);
    UnloadSound(playerDeathSound);
    UnloadSound(playerHitSound);
    UnloadTexture(lightMaskTexture);
    UnloadTexture(pianoPanelMask);
    UnloadTexture(caveTexture);
    UnloadTexture(playerTexture);
    for (int i = 0; i < PLAYER_ATTACK_FRAME_COUNT; i++) {
        UnloadTexture(playerAttackFrames[i]);
    }
    UnloadTexture(playerBloatedTexture);
    for (int i = 0; i < PLAYER_JUMP_FRAME_COUNT; i++) {
        UnloadTexture(playerJumpFrames[i]);
    }
    for (int i = 0; i < PLAYER_SLAM_FRAME_COUNT; i++) {
        UnloadTexture(playerSlamFrames[i]);
    }
    for (int i = 0; i < PLAYER_DOUBLE_JUMP_FRAME_COUNT; i++) {
        UnloadTexture(playerDoubleJumpFrames[i]);
    }
    for (int i = 0; i < PLAYER_CAST_FRAME_COUNT; i++) {
        UnloadTexture(playerCastFireballFrames[i]);
        UnloadTexture(playerCastFrostFrames[i]);
    }
    UnloadTexture(fireballTexture);
#if !defined(PLATFORM_IOS)
    UnloadRenderTexture(canvas);
#endif
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
