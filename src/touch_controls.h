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

void TouchControlsInit(float virtualWidth, float virtualHeight);
// camera is whatever's currently mapping the virtual canvas to the real
// screen (zoom + offset); used to convert touch positions (screen space)
// into virtual-canvas space so hit-testing lines up with what's drawn.
void TouchControlsUpdate(float dt, Camera2D camera);
void TouchControlsDraw();

// (0,0) when the joystick isn't currently held, otherwise each axis is in [-1, 1].
Vector2 TouchControlsMoveDirection();
bool TouchControlsJumpPressed();   // true only on the frame the button is newly pressed
bool TouchControlsAttackPressed(); // true only on the frame the button is newly pressed

// Mirrors GetNotesPressed()'s contract (chord_system.h) so it composes the
// same way in the note-handling loop: fills outNotes with every note
// struck on the chord wheel since the last call, returns how many were found.
int TouchControlsNotesPressed(Note *outNotes, int maxNotes);
