#include "chord_system.h"
#include "raylib.h"

static const NoteKeyBinding NOTE_KEY_BINDINGS[NOTE_KEY_COUNT] = {
    {Note::C, KEY_Q, "C"},
    {Note::D, KEY_A, "D"},
    {Note::E, KEY_W, "E"},
    {Note::F, KEY_S, "F"},
    {Note::G, KEY_E, "G"},
    {Note::A, KEY_D, "A"},
    {Note::B, KEY_R, "B"},
};

const NoteKeyBinding *GetNoteKeyBindings() {
    return NOTE_KEY_BINDINGS;
}

int GetNotesPressed(Note *outNotes, int maxNotes) {
    int count = 0;
    for (int i = 0; i < NOTE_KEY_COUNT && count < maxNotes; i++) {
        if (IsKeyPressed(NOTE_KEY_BINDINGS[i].key)) {
            outNotes[count++] = NOTE_KEY_BINDINGS[i].note;
        }
    }
    return count;
}

const char *NoteToString(Note note) {
    switch (note) {
        case Note::C:       return "C";
        case Note::C_SHARP: return "C#";
        case Note::D:       return "D";
        case Note::D_SHARP: return "D#";
        case Note::E:       return "E";
        case Note::F:       return "F";
        case Note::F_SHARP: return "F#";
        case Note::G:       return "G";
        case Note::G_SHARP: return "G#";
        case Note::A:       return "A";
        case Note::A_SHARP: return "A#";
        case Note::B:       return "B";
        default:            return "-";
    }
}

float NoteFrequency(Note note) {
    switch (note) {
        case Note::C: return 130.81f;
        case Note::D: return 146.83f;
        case Note::E: return 164.81f;
        case Note::F: return 174.61f;
        case Note::G: return 196.00f;
        case Note::A: return 220.00f;
        case Note::B: return 246.94f;
        default:      return 0.0f;
    }
}

const char *SpellToString(Spell spell) {
    switch (spell) {
        case Spell::FIREBALL: return "FIRE";
        case Spell::LIGHT:    return "BEAM";
        case Spell::DASH:     return "DASH";
        case Spell::FROST:    return "FROST";
        default:              return "";
    }
}

struct ChordDefinition {
    Note notes[MAX_CHORD_SIZE];
    int length;
    Spell resultSpell;
};

// Fireball: F, A, C. Light: B, D, F (B diminished triad). Dash: D, F, A
// (D minor triad -- D major would need F#, unavailable with natural notes only).
// Frost: C, E, G (C major triad).
static const ChordDefinition CHORD_DEFINITIONS[4] = {
    {{Note::F, Note::A, Note::C}, 3, Spell::FIREBALL},
    {{Note::B, Note::D, Note::F}, 3, Spell::LIGHT},
    {{Note::D, Note::F, Note::A}, 3, Spell::DASH},
    {{Note::C, Note::E, Note::G}, 3, Spell::FROST},
};
static const int CHORD_DEFINITION_COUNT = 4;
static const double BURST_GAP_SECONDS = 0.25;

int GetChordDefinitionCount() {
    return CHORD_DEFINITION_COUNT;
}

void GetChordDefinition(int index, Spell &outSpell, const Note *&outNotes, int &outLength) {
    outSpell = CHORD_DEFINITIONS[index].resultSpell;
    outNotes = CHORD_DEFINITIONS[index].notes;
    outLength = CHORD_DEFINITIONS[index].length;
}

ChordBurst ChordBurstCreate() {
    return {{}, 0, 0.0};
}

static bool BurstContains(const ChordBurst &burst, Note note) {
    for (int i = 0; i < burst.count; i++) {
        if (burst.notes[i] == note) return true;
    }
    return false;
}

static Spell EvaluateBurst(const ChordBurst &burst) {
    for (int d = 0; d < CHORD_DEFINITION_COUNT; d++) {
        const ChordDefinition &def = CHORD_DEFINITIONS[d];
        if (burst.count != def.length) continue;

        bool allMatch = true;
        for (int i = 0; i < def.length; i++) {
            if (!BurstContains(burst, def.notes[i])) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) return def.resultSpell;
    }
    return Spell::NONE;
}

Spell ChordBurstAddNote(ChordBurst &burst, Note note) {
    burst.notes[burst.count] = note;
    burst.count++;
    burst.lastNoteTime = GetTime();

    if (burst.count >= MAX_CHORD_SIZE) {
        Spell result = EvaluateBurst(burst);
        burst.count = 0;
        return result;
    }
    return Spell::NONE;
}

Spell ChordBurstUpdate(ChordBurst &burst) {
    if (burst.count == 0) return Spell::NONE;

    if (GetTime() - burst.lastNoteTime > BURST_GAP_SECONDS) {
        Spell result = EvaluateBurst(burst);
        burst.count = 0;
        return result;
    }
    return Spell::NONE;
}
