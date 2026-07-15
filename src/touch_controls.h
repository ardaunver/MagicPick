#pragma once
#include "raylib.h"
#include "chord_system.h"

// Touch UI for mobile: a movement joystick, jump/attack buttons, and the
// note "chord wheel" gesture input. A single global instance -- there's
// only ever one set of touch controls in this game -- exposing simple
// query functions gameplay code (player.cpp, main.cpp) reads the same way
// it already reads keyboard state, so touch and keyboard drive the same
// code paths instead of duplicating logic per platform.
static const int CHORD_WHEEL_GATE_COUNT = 7; // C D E F G A B

// Maps screen-space touch positions to virtual-canvas space. Not raylib's
// Camera2D: that only supports a single uniform zoom, but iOS (fixed aspect,
// letterboxed) and Android (stretched to fill the screen edge-to-edge) need
// independent X/Y scale factors, and only iOS needs the 180-degree flip
// (compensating for its vendored fork's own rendering quirk -- see main.cpp).
struct ScreenFit {
    float scaleX, scaleY;   // virtual-canvas -> screen scale
    float offsetX, offsetY; // screen-space offset applied after scaling
    bool rotated180;
};

void TouchControlsInit(float virtualWidth, float virtualHeight);
void TouchControlsUpdate(float dt, ScreenFit fit);
void TouchControlsDraw();

// (0,0) when the joystick isn't currently held, otherwise each axis is in [-1, 1].
Vector2 TouchControlsMoveDirection();
bool TouchControlsJumpPressed();   // true only on the frame the button is newly pressed
bool TouchControlsAttackPressed(); // true only on the frame the button is newly pressed

// Mirrors GetNotesPressed()'s contract (chord_system.h) so it composes the
// same way in the note-handling loop: fills outNotes with every note
// struck on the chord wheel since the last call, returns how many were found.
int TouchControlsNotesPressed(Note *outNotes, int maxNotes);

// For simple one-shot UI button taps (title screen, menus): true on the
// frame a new touch begins anywhere on screen, with its position converted
// to virtual-canvas space via fit. Deliberately bypasses
// GetMousePosition()/IsMouseButtonPressed() on iOS -- that fork synthesizes
// those from touch input across a thread hand-off (UIKit's touch callbacks
// vs. raylib's own render thread) that doesn't fire reliably, whereas
// GetTouchPointCount()/GetTouchPosition() (used here, and by every other
// control in this file) does.
bool TouchControlsTapped(ScreenFit fit, Vector2 *outVirtualPos);
