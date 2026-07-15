#include "touch_controls.h"
#include <cmath>
#include <cstdio>

static const float JOYSTICK_ACTIVATE_RADIUS_MULT = 1.6f; // how far from the base a touch can start and still grab it
static const float GATE_GROWN_SCALE = 1.6f;
static const float GATE_ANIM_SPEED = 12.0f; // higher = snappier grow/shrink

TouchControls TouchControlsCreate(float virtualWidth, float virtualHeight) {
    TouchControls tc = {};

    tc.joystickBase = {virtualWidth * 0.125f, virtualHeight * 0.72f};
    tc.joystickRadius = virtualHeight * 0.12f;
    tc.joystickTouchIndex = -1;

    tc.wheelCenter = {virtualWidth * 0.8f, virtualHeight * 0.47f};
    tc.wheelGateDistance = virtualHeight * 0.18f;
    tc.gateHitRadius = virtualHeight * 0.085f;
    tc.wheelTouchIndex = -1;
    for (int i = 0; i < CHORD_WHEEL_GATE_COUNT; i++) {
        tc.gateScale[i] = 1.0f;
    }
    tc.gateNotes[0] = Note::C;
    tc.gateNotes[1] = Note::D;
    tc.gateNotes[2] = Note::E;
    tc.gateNotes[3] = Note::F;
    tc.gateNotes[4] = Note::G;
    tc.gateNotes[5] = Note::A;
    tc.gateNotes[6] = Note::B;
    tc.lastNotesText[0] = '\0';

    float buttonWidth = virtualWidth * 0.0875f;
    float buttonHeight = virtualHeight * 0.1f;
    float buttonY = virtualHeight * 0.845f;
    tc.jumpButtonRect = {virtualWidth * 0.766f, buttonY, buttonWidth, buttonHeight};
    tc.attackButtonRect = {virtualWidth * 0.87f, buttonY, buttonWidth, buttonHeight};
    tc.jumpTouchIndex = -1;
    tc.attackTouchIndex = -1;

    return tc;
}

static Vector2 GateCenter(const TouchControls &tc, int i) {
    float angle = (-90.0f + i * (360.0f / CHORD_WHEEL_GATE_COUNT)) * DEG2RAD;
    return {
        tc.wheelCenter.x + cosf(angle) * tc.wheelGateDistance,
        tc.wheelCenter.y + sinf(angle) * tc.wheelGateDistance
    };
}

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

void TouchControlsUpdate(TouchControls &tc, float dt, float virtualScale) {
    int touchCount = GetTouchPointCount();
    Vector2 touchPos[16];
    bool touchClaimed[16] = {};
    if (touchCount > 16) touchCount = 16;
    for (int i = 0; i < touchCount; i++) {
        Vector2 screenPos = GetTouchPosition(i);
        touchPos[i] = {screenPos.x / virtualScale, screenPos.y / virtualScale};
    }

    // Joystick: keep controlling the same touch index until it lifts.
    if (tc.joystickTouchIndex >= touchCount) tc.joystickTouchIndex = -1;
    if (tc.joystickTouchIndex == -1) {
        for (int i = 0; i < touchCount; i++) {
            if (touchClaimed[i]) continue;
            if (Dist(touchPos[i], tc.joystickBase) <= tc.joystickRadius * JOYSTICK_ACTIVATE_RADIUS_MULT) {
                tc.joystickTouchIndex = i;
                break;
            }
        }
    }
    if (tc.joystickTouchIndex != -1) {
        touchClaimed[tc.joystickTouchIndex] = true;
        Vector2 offset = {
            touchPos[tc.joystickTouchIndex].x - tc.joystickBase.x,
            touchPos[tc.joystickTouchIndex].y - tc.joystickBase.y
        };
        float len = sqrtf(offset.x * offset.x + offset.y * offset.y);
        if (len > tc.joystickRadius && len > 0.0f) {
            offset.x = offset.x / len * tc.joystickRadius;
            offset.y = offset.y / len * tc.joystickRadius;
            len = tc.joystickRadius;
        }
        tc.joystickKnobOffset = offset;
        tc.moveDirection = {offset.x / tc.joystickRadius, offset.y / tc.joystickRadius};
    } else {
        tc.joystickKnobOffset = {0, 0};
        tc.moveDirection = {0, 0};
    }

    // Jump / attack buttons: held for as long as any unclaimed touch sits on them.
    tc.jumpHeld = false;
    if (tc.jumpTouchIndex >= touchCount) tc.jumpTouchIndex = -1;
    for (int i = 0; i < touchCount; i++) {
        if (touchClaimed[i]) continue;
        if (CheckCollisionPointRec(touchPos[i], tc.jumpButtonRect)) {
            tc.jumpHeld = true;
            tc.jumpTouchIndex = i;
            touchClaimed[i] = true;
            break;
        }
    }

    tc.attackHeld = false;
    if (tc.attackTouchIndex >= touchCount) tc.attackTouchIndex = -1;
    for (int i = 0; i < touchCount; i++) {
        if (touchClaimed[i]) continue;
        if (CheckCollisionPointRec(touchPos[i], tc.attackButtonRect)) {
            tc.attackHeld = true;
            tc.attackTouchIndex = i;
            touchClaimed[i] = true;
            break;
        }
    }

    // Chord wheel: claim a touch anywhere near the wheel, then track which
    // gates it's inside frame to frame. Crossing a gate's boundary (either
    // direction) is the "strum" moment -- resting inside/outside does nothing.
    if (tc.wheelTouchIndex >= touchCount) tc.wheelTouchIndex = -1;
    if (tc.wheelTouchIndex == -1) {
        for (int i = 0; i < touchCount; i++) {
            if (touchClaimed[i]) continue;
            if (Dist(touchPos[i], tc.wheelCenter) <= tc.wheelGateDistance + tc.gateHitRadius * 1.5f) {
                tc.wheelTouchIndex = i;
                for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
                    tc.noteCrossedThisTouch[g] = false;
                }
                tc.lastNotesText[0] = '\0';
                break;
            }
        }
    }

    bool wheelHeld = (tc.wheelTouchIndex != -1);
    Vector2 wheelTouchPos = wheelHeld ? touchPos[tc.wheelTouchIndex] : Vector2{0, 0};

    for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
        bool nowInside = wheelHeld && (Dist(wheelTouchPos, GateCenter(tc, g)) <= tc.gateHitRadius);
        if (nowInside != tc.gateInside[g]) {
            tc.noteCrossedThisTouch[g] = true; // crossed the boundary this frame, either direction
        }
        tc.gateInside[g] = nowInside;

        float target = nowInside ? GATE_GROWN_SCALE : 1.0f;
        tc.gateScale[g] += (target - tc.gateScale[g]) * fminf(1.0f, dt * GATE_ANIM_SPEED);
    }

    if (wheelHeld) {
        int pos = 0;
        for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
            if (tc.noteCrossedThisTouch[g]) {
                pos += snprintf(tc.lastNotesText + pos, sizeof(tc.lastNotesText) - pos, "%s ", NoteToString(tc.gateNotes[g]));
            }
        }
    } else {
        for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) tc.gateInside[g] = false;
    }
}

void TouchControlsDraw(const TouchControls &tc) {
    // Joystick
    DrawCircleV(tc.joystickBase, tc.joystickRadius, Fade(RAYWHITE, 0.25f));
    DrawCircleLines((int)tc.joystickBase.x, (int)tc.joystickBase.y, tc.joystickRadius, Fade(BLACK, 0.6f));
    Vector2 knobPos = {tc.joystickBase.x + tc.joystickKnobOffset.x, tc.joystickBase.y + tc.joystickKnobOffset.y};
    DrawCircleV(knobPos, tc.joystickRadius * 0.45f, Fade(RAYWHITE, 0.8f));
    DrawCircleLines((int)knobPos.x, (int)knobPos.y, tc.joystickRadius * 0.45f, BLACK);

    // Jump / attack buttons
    DrawRectangleRec(tc.jumpButtonRect, tc.jumpHeld ? Fade(YELLOW, 0.7f) : Fade(RAYWHITE, 0.35f));
    DrawRectangleLinesEx(tc.jumpButtonRect, 1.0f, BLACK);
    const char *jumpLabel = "JUMP";
    DrawText(jumpLabel, (int)(tc.jumpButtonRect.x + (tc.jumpButtonRect.width - MeasureText(jumpLabel, 8)) / 2),
        (int)(tc.jumpButtonRect.y + (tc.jumpButtonRect.height - 8) / 2), 8, BLACK);

    DrawRectangleRec(tc.attackButtonRect, tc.attackHeld ? Fade(ORANGE, 0.7f) : Fade(RAYWHITE, 0.35f));
    DrawRectangleLinesEx(tc.attackButtonRect, 1.0f, BLACK);
    const char *attackLabel = "ATTACK";
    DrawText(attackLabel, (int)(tc.attackButtonRect.x + (tc.attackButtonRect.width - MeasureText(attackLabel, 8)) / 2),
        (int)(tc.attackButtonRect.y + (tc.attackButtonRect.height - 8) / 2), 8, BLACK);

    // Chord wheel
    DrawCircleV(tc.wheelCenter, tc.wheelGateDistance * 0.35f, Fade(RAYWHITE, 0.12f)); // neutral middle, just a visual hint
    for (int g = 0; g < CHORD_WHEEL_GATE_COUNT; g++) {
        Vector2 gc = GateCenter(tc, g);
        float r = tc.gateHitRadius * tc.gateScale[g];
        Color fill = tc.gateInside[g] ? Fade(YELLOW, 0.85f) : Fade(RAYWHITE, 0.4f);
        DrawCircleV(gc, r, fill);
        DrawCircleLines((int)gc.x, (int)gc.y, r, BLACK);
        const char *label = NoteToString(tc.gateNotes[g]);
        DrawText(label, (int)(gc.x - MeasureText(label, 10) / 2.0f), (int)(gc.y - 5), 10, BLACK);
    }

    if (tc.lastNotesText[0] != '\0') {
        DrawText(tc.lastNotesText, (int)(tc.wheelCenter.x - tc.wheelGateDistance), (int)(tc.wheelCenter.y + tc.wheelGateDistance + tc.gateHitRadius + 4), 8, RAYWHITE);
    }
}
