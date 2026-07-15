#include "touch_controls.h"
#include <cmath>
#include <cstdio>

static const float JOYSTICK_ACTIVATE_RADIUS_MULT = 1.6f; // how far from the base a touch can start and still grab it
static const float GATE_GROWN_SCALE = 1.6f;
static const float GATE_ANIM_SPEED = 12.0f; // higher = snappier grow/shrink

struct TouchControlsState {
    // Movement joystick (bottom-left).
    Vector2 joystickBase;
    float joystickRadius;
    Vector2 joystickKnobOffset;
    int joystickTouchIndex; // -1 when not held
    Vector2 moveDirection;

    // Jump / attack buttons (lower-right, below the chord wheel).
    Rectangle jumpButtonRect;
    Rectangle attackButtonRect;
    bool jumpHeld;
    bool attackHeld;
    bool jumpHeldPrevFrame;
    bool attackHeldPrevFrame;
    int jumpTouchIndex;
    int attackTouchIndex;

    // Chord wheel (upper-right). Gates are the 7 natural notes evenly spaced
    // in scale order around wheelCenter. A note "plays" (the string-strum
    // moment) when a touch crosses into or out of a gate's hit circle --
    // resting inside or outside doesn't trigger anything, only the crossing
    // does. The middle and the space beyond the gates are neutral: a touch
    // can duck through the center to reach a non-adjacent gate without
    // triggering the gates in between.
    Vector2 wheelCenter;
    float wheelGateDistance; // distance from center to each gate's center
    float gateHitRadius;     // radius of each gate's own hit circle
    float gateScale[CHORD_WHEEL_GATE_COUNT];  // animated visual scale, 1.0 = resting
    bool gateInside[CHORD_WHEEL_GATE_COUNT];  // touch currently within this gate's circle
    bool noteCrossedThisTouch[CHORD_WHEEL_GATE_COUNT]; // notes struck since the wheel touch began (debug display)
    int wheelTouchIndex; // -1 when not held
    char lastNotesText[32]; // debug readout of the most recent (or in-progress) set

    Note gateNotes[CHORD_WHEEL_GATE_COUNT];

    // Notes struck on THIS update call only, for feeding into the chord
    // system the same frame they happen (mirrors GetNotesPressed()'s "this
    // frame" contract).
    Note framedNotes[CHORD_WHEEL_GATE_COUNT];
    int framedNoteCount;
};

static TouchControlsState g_tc;

void TouchControlsInit(float virtualWidth, float virtualHeight) {
    g_tc = {};

    g_tc.joystickBase = {virtualWidth * 0.125f, virtualHeight * 0.72f};
    g_tc.joystickRadius = virtualHeight * 0.12f;
    g_tc.joystickTouchIndex = -1;

    g_tc.wheelCenter = {virtualWidth * 0.8f, virtualHeight * 0.47f};
    g_tc.wheelGateDistance = virtualHeight * 0.18f;
    g_tc.gateHitRadius = virtualHeight * 0.085f;
    g_tc.wheelTouchIndex = -1;
    for (int i = 0; i < CHORD_WHEEL_GATE_COUNT; i++) {
        g_tc.gateScale[i] = 1.0f;
    }
    g_tc.gateNotes[0] = Note::C;
    g_tc.gateNotes[1] = Note::D;
    g_tc.gateNotes[2] = Note::E;
    g_tc.gateNotes[3] = Note::F;
    g_tc.gateNotes[4] = Note::G;
    g_tc.gateNotes[5] = Note::A;
    g_tc.gateNotes[6] = Note::B;
    g_tc.lastNotesText[0] = '\0';

    float buttonWidth = virtualWidth * 0.0875f;
    float buttonHeight = virtualHeight * 0.1f;
    float buttonY = virtualHeight * 0.845f;
    g_tc.jumpButtonRect = {virtualWidth * 0.766f, buttonY, buttonWidth, buttonHeight};
    g_tc.attackButtonRect = {virtualWidth * 0.87f, buttonY, buttonWidth, buttonHeight};
    g_tc.jumpTouchIndex = -1;
    g_tc.attackTouchIndex = -1;
}

static Vector2 GateCenter(int i) {
    float angle = (-90.0f + i * (360.0f / CHORD_WHEEL_GATE_COUNT)) * DEG2RAD;
    return {
        g_tc.wheelCenter.x + cosf(angle) * g_tc.wheelGateDistance,
        g_tc.wheelCenter.y + sinf(angle) * g_tc.wheelGateDistance
    };
}

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

void TouchControlsUpdate(float dt, Camera2D camera) {
    g_tc.framedNoteCount = 0;

    int touchCount = GetTouchPointCount();
    Vector2 touchPos[16];
    bool touchClaimed[16] = {};
    if (touchCount > 16) touchCount = 16;
    for (int i = 0; i < touchCount; i++) {
        Vector2 screenPos = GetTouchPosition(i);
        touchPos[i] = GetScreenToWorld2D(screenPos, camera);
    }

    // Joystick: keep controlling the same touch index until it lifts.
    if (g_tc.joystickTouchIndex >= touchCount) g_tc.joystickTouchIndex = -1;
    if (g_tc.joystickTouchIndex == -1) {
        for (int i = 0; i < touchCount; i++) {
            if (touchClaimed[i]) continue;
            if (Dist(touchPos[i], g_tc.joystickBase) <= g_tc.joystickRadius * JOYSTICK_ACTIVATE_RADIUS_MULT) {
                g_tc.joystickTouchIndex = i;
                break;
            }
        }
    }
    if (g_tc.joystickTouchIndex != -1) {
        touchClaimed[g_tc.joystickTouchIndex] = true;
        Vector2 offset = {
            touchPos[g_tc.joystickTouchIndex].x - g_tc.joystickBase.x,
            touchPos[g_tc.joystickTouchIndex].y - g_tc.joystickBase.y
        };
        float len = sqrtf(offset.x * offset.x + offset.y * offset.y);
        if (len > g_tc.joystickRadius && len > 0.0f) {
            offset.x = offset.x / len * g_tc.joystickRadius;
            offset.y = offset.y / len * g_tc.joystickRadius;
        }
        g_tc.joystickKnobOffset = offset;
        g_tc.moveDirection = {offset.x / g_tc.joystickRadius, offset.y / g_tc.joystickRadius};
    } else {
        g_tc.joystickKnobOffset = {0, 0};
        g_tc.moveDirection = {0, 0};
    }

    // Jump / attack buttons: held for as long as any unclaimed touch sits on them.
    g_tc.jumpHeldPrevFrame = g_tc.jumpHeld;
    g_tc.jumpHeld = false;
    if (g_tc.jumpTouchIndex >= touchCount) g_tc.jumpTouchIndex = -1;
    for (int i = 0; i < touchCount; i++) {
        if (touchClaimed[i]) continue;
        if (CheckCollisionPointRec(touchPos[i], g_tc.jumpButtonRect)) {
            g_tc.jumpHeld = true;
            g_tc.jumpTouchIndex = i;
            touchClaimed[i] = true;
            break;
        }
    }

    g_tc.attackHeldPrevFrame = g_tc.attackHeld;
    g_tc.attackHeld = false;
    if (g_tc.attackTouchIndex >= touchCount) g_tc.attackTouchIndex = -1;
    for (int i = 0; i < touchCount; i++) {
        if (touchClaimed[i]) continue;
        if (CheckCollisionPointRec(touchPos[i], g_tc.attackButtonRect)) {
            g_tc.attackHeld = true;
            g_tc.attackTouchIndex = i;
            touchClaimed[i] = true;
            break;
        }
    }

    // Chord wheel: claim a touch anywhere near the wheel, then track which
    // gates it's inside frame to frame. Crossing a gate's boundary (either
    // direction) is the "strum" moment -- resting inside/outside does nothing.
    if (g_tc.wheelTouchIndex >= touchCount) g_tc.wheelTouchIndex = -1;
    if (g_tc.wheelTouchIndex == -1) {
        for (int i = 0; i < touchCount; i++) {
            if (touchClaimed[i]) continue;
            if (Dist(touchPos[i], g_tc.wheelCenter) <= g_tc.wheelGateDistance + g_tc.gateHitRadius * 1.5f) {
                g_tc.wheelTouchIndex = i;
                for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
                    g_tc.noteCrossedThisTouch[g] = false;
                }
                g_tc.lastNotesText[0] = '\0';
                break;
            }
        }
    }

    bool wheelHeld = (g_tc.wheelTouchIndex != -1);
    Vector2 wheelTouchPos = wheelHeld ? touchPos[g_tc.wheelTouchIndex] : Vector2{0, 0};

    for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
        bool nowInside = wheelHeld && (Dist(wheelTouchPos, GateCenter(g)) <= g_tc.gateHitRadius);
        if (nowInside != g_tc.gateInside[g]) {
            g_tc.noteCrossedThisTouch[g] = true; // crossed the boundary this frame, either direction
            if (g_tc.framedNoteCount < CHORD_WHEEL_GATE_COUNT) {
                g_tc.framedNotes[g_tc.framedNoteCount++] = g_tc.gateNotes[g];
            }
        }
        g_tc.gateInside[g] = nowInside;

        float target = nowInside ? GATE_GROWN_SCALE : 1.0f;
        g_tc.gateScale[g] += (target - g_tc.gateScale[g]) * fminf(1.0f, dt * GATE_ANIM_SPEED);
    }

    if (wheelHeld) {
        int pos = 0;
        for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
            if (g_tc.noteCrossedThisTouch[g]) {
                pos += snprintf(g_tc.lastNotesText + pos, sizeof(g_tc.lastNotesText) - pos, "%s ", NoteToString(g_tc.gateNotes[g]));
            }
        }
    } else {
        for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) g_tc.gateInside[g] = false;
    }
}

Vector2 TouchControlsMoveDirection() {
    return g_tc.moveDirection;
}

bool TouchControlsJumpPressed() {
    return g_tc.jumpHeld && !g_tc.jumpHeldPrevFrame;
}

bool TouchControlsAttackPressed() {
    return g_tc.attackHeld && !g_tc.attackHeldPrevFrame;
}

int TouchControlsNotesPressed(Note *outNotes, int maxNotes) {
    int count = (g_tc.framedNoteCount < maxNotes) ? g_tc.framedNoteCount : maxNotes;
    for (int i = 0; i < count; i++) outNotes[i] = g_tc.framedNotes[i];
    return count;
}

void TouchControlsDraw() {
    // Joystick
    DrawCircleV(g_tc.joystickBase, g_tc.joystickRadius, Fade(RAYWHITE, 0.25f));
    DrawCircleLines((int)g_tc.joystickBase.x, (int)g_tc.joystickBase.y, g_tc.joystickRadius, Fade(BLACK, 0.6f));
    Vector2 knobPos = {g_tc.joystickBase.x + g_tc.joystickKnobOffset.x, g_tc.joystickBase.y + g_tc.joystickKnobOffset.y};
    DrawCircleV(knobPos, g_tc.joystickRadius * 0.45f, Fade(RAYWHITE, 0.8f));
    DrawCircleLines((int)knobPos.x, (int)knobPos.y, g_tc.joystickRadius * 0.45f, BLACK);

    // Jump / attack buttons
    DrawRectangleRec(g_tc.jumpButtonRect, g_tc.jumpHeld ? Fade(YELLOW, 0.7f) : Fade(RAYWHITE, 0.35f));
    DrawRectangleLinesEx(g_tc.jumpButtonRect, 1.0f, BLACK);
    const char *jumpLabel = "JUMP";
    DrawText(jumpLabel, (int)(g_tc.jumpButtonRect.x + (g_tc.jumpButtonRect.width - MeasureText(jumpLabel, 8)) / 2),
        (int)(g_tc.jumpButtonRect.y + (g_tc.jumpButtonRect.height - 8) / 2), 8, BLACK);

    DrawRectangleRec(g_tc.attackButtonRect, g_tc.attackHeld ? Fade(ORANGE, 0.7f) : Fade(RAYWHITE, 0.35f));
    DrawRectangleLinesEx(g_tc.attackButtonRect, 1.0f, BLACK);
    const char *attackLabel = "ATTACK";
    DrawText(attackLabel, (int)(g_tc.attackButtonRect.x + (g_tc.attackButtonRect.width - MeasureText(attackLabel, 8)) / 2),
        (int)(g_tc.attackButtonRect.y + (g_tc.attackButtonRect.height - 8) / 2), 8, BLACK);

    // Chord wheel
    DrawCircleV(g_tc.wheelCenter, g_tc.wheelGateDistance * 0.35f, Fade(RAYWHITE, 0.12f)); // neutral middle, just a visual hint
    for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
        Vector2 gc = GateCenter(g);
        float r = g_tc.gateHitRadius * g_tc.gateScale[g];
        Color fill = g_tc.gateInside[g] ? Fade(YELLOW, 0.85f) : Fade(RAYWHITE, 0.4f);
        DrawCircleV(gc, r, fill);
        DrawCircleLines((int)gc.x, (int)gc.y, r, BLACK);
        const char *label = NoteToString(g_tc.gateNotes[g]);
        DrawText(label, (int)(gc.x - MeasureText(label, 10) / 2.0f), (int)(gc.y - 5), 10, BLACK);
    }

    if (g_tc.lastNotesText[0] != '\0') {
        DrawText(g_tc.lastNotesText, (int)(g_tc.wheelCenter.x - g_tc.wheelGateDistance), (int)(g_tc.wheelCenter.y + g_tc.wheelGateDistance + g_tc.gateHitRadius + 4), 8, RAYWHITE);
    }
}
