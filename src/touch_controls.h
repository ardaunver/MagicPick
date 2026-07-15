#pragma once
#include "raylib.h"
#include "chord_system.h"

// Initial-pass touch UI for mobile: a movement joystick, jump/attack
// buttons, and the note "chord wheel" gesture input. Purely reads touch
// input and updates its own visuals -- not yet wired to player movement,
// jumping, attacking, or spellcasting. That's a follow-up integration step
// once the feel of these controls is tuned.
static const int CHORD_WHEEL_GATE_COUNT = 7; // C D E F G A B

struct TouchControls {
    // Movement joystick (bottom-left). moveDirection is (0,0) when not held,
    // otherwise each axis is in [-1, 1].
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
    bool noteCrossedThisTouch[CHORD_WHEEL_GATE_COUNT]; // notes struck since the wheel touch began
    int wheelTouchIndex; // -1 when not held
    char lastNotesText[32]; // debug readout of the most recent (or in-progress) set

    Note gateNotes[CHORD_WHEEL_GATE_COUNT];
};

TouchControls TouchControlsCreate(float virtualWidth, float virtualHeight);
void TouchControlsUpdate(TouchControls &tc, float dt, float virtualScale);
void TouchControlsDraw(const TouchControls &tc);
