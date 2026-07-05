#pragma once

enum class Note {
    NONE = -1,
    C, C_SHARP, D, D_SHARP, E, F, F_SHARP, G, G_SHARP, A, A_SHARP, B
};

// A natural note bound to a keyboard key, used for both input and the
// on-screen piano key display so the two never drift apart.
struct NoteKeyBinding {
    Note note;
    int key; // raylib KeyboardKey
    const char *label;
};

static const int NOTE_KEY_COUNT = 7;
const NoteKeyBinding *GetNoteKeyBindings();

// Fills outNotes with every note whose key was pressed this frame (supports
// playing several keys of a chord shape at once). Returns how many were found.
int GetNotesPressed(Note *outNotes, int maxNotes);
const char *NoteToString(Note note);
// Frequency in Hz (4th octave) for the 7 natural notes; 0 for anything else.
float NoteFrequency(Note note);

enum class Spell {
    NONE,
    FIREBALL,
    LIGHT,
    DASH,
    FROST
};

const char *SpellToString(Spell spell);

static const int MAX_CHORD_SIZE = 3;

int GetChordDefinitionCount();
// Notes and spell for the chord definition at index (0..GetChordDefinitionCount()-1).
void GetChordDefinition(int index, Spell &outSpell, const Note *&outNotes, int &outLength);

// Groups notes played close together in time into a "burst" (a hand shape),
// order-independent, and evaluates the resulting set once the burst closes
// (either it fills up or the player pauses past the gap threshold).
struct ChordBurst {
    Note notes[MAX_CHORD_SIZE];
    int count;
    double lastNoteTime;
};

ChordBurst ChordBurstCreate();
// Call when a note is played; resolves immediately if the burst just filled up.
Spell ChordBurstAddNote(ChordBurst &burst, Note note);
// Call once per frame regardless of input; resolves a pending burst that has
// gone idle past the gap threshold.
Spell ChordBurstUpdate(ChordBurst &burst);
