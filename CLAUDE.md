# Project: Guitarist-Sorcerer (working title)

## Concept
A 2D pixel-art action game. The player is a guitarist who is also a sorcerer.
The guitar is both an instrument and a weapon, and the player switches between
two stances:

- **Sorcerer / Musician stance**: guitar held normally (strap, standing position).
  In this stance the player can play notes to cast spells.
- **Fighter stance**: guitar held by the neck like an axe/club.
  In this stance the player can melee attack (and later, parry).

## Core mechanics (long-term vision, not all in MVP)
- Playing single notes in a specific order forms a **chord**, and each chord is
  a **spell**.
- Chromatic notes: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
- Example: F major chord = F, A, C -> casts **Fireball** (F for Fire, easy mnemonic
  during early development; can be renamed/reworked later).
- Later: minor chords, 7th chords, more spells, parry while in Fighter stance.

## MVP scope (build this first, nothing more)
1. Player entity, 2D, pixel-art style (use placeholder colored rectangles/circles
   for now — do NOT spend time on real art yet).
2. Movement: Arrow keys (Left/Right) move the character; Up/Down ignored for now.
3. Jump: Space bar. Simple gravity + single jump, no double-jump yet.
4. Stance switch: "Q" toggles between Sorcerer stance and Fighter stance.
   - Visually distinguish stances with a simple color or shape change on the
     placeholder sprite (e.g. guitar-neck-down icon vs guitar-neck-up icon,
     or just a different tint) until real art exists.
5. Fighter stance: pressing a designated attack key (let's use "F" — separate
   from the note keys below since notes only matter in Sorcerer stance) triggers
   a simple melee swing: a short hitbox in front of the player for ~0.2s,
   with a basic animation state (even if it's just a placeholder swing rectangle).
6. Sorcerer stance: note input via number row keys, mapped chromatically:
   - `1` = C
   - `2` = C#
   - `3` = D
   - `4` = D#
   - `5` = E
   - `6` = F
   - `7` = F#
   - `8` = G
   - `9` = G#
   - `0` = A
   - `-` = A#
   - `=` = B
   - Note keys are only "live" while in Sorcerer stance. Pressing them in
     Fighter stance does nothing (or optionally could feed into a future
     "instrument on cooldown" idea — ignore for now).
7. Chord detection (Fireball only, for MVP):
   - Track the last few notes played with timestamps.
   - If the player plays F, then A, then C, **in that order**, within a rolling
     window of 600ms total, trigger the Fireball spell event.
   - On trigger: for now, just print/log "FIREBALL CAST" to console and flash
     the player sprite (e.g. tint orange for 0.3s). No projectile yet — that's
     a follow-up milestone, not MVP.
   - If the sequence breaks (wrong note, or timeout exceeded) reset the tracked
     sequence.
8. Basic game loop: 60 FPS target, raylib window ~960x540 (scaled 3x from a
   320x180 virtual pixel-art canvas, or similar — render to a low-res render
   texture and scale up for a crisp pixel-art look; don't render pixel art
   directly at native window resolution).

## Explicitly OUT of scope for MVP (don't build yet)
- Real pixel-art sprites/animations (placeholders only)
- Level design / tilemaps / Tiled integration
- Parry mechanic
- Minor/7th chords or any spell besides Fireball
- Actual fireball projectile/damage/enemies
- Sound/music (ironic for a guitar game, but not yet)
- Menus, UI, HUD

## Technical stack
- Language: C++17
- Library: raylib (installed via Homebrew: `brew install raylib`)
- Build: CMake
- Platform: macOS (Apple Silicon, M1) — native build, no Rosetta needed
- Target: single executable, simple flat project structure for now:
  ```
  /src
    main.cpp
    player.h / player.cpp
    chord_system.h / chord_system.cpp
  /assets        (empty for now, placeholders drawn in code)
  CMakeLists.txt
  ```

## Code style preferences
- Prefer inline code over unnecessary helper-function wrappers for simple logic.
- Use explicit array/struct references rather than heavy abstraction layers.
- Prefer `switch-case` over long `if-else if` chains (e.g. for note-to-enum
  mapping, stance state, etc.).
- Keep it simple and readable — this is an early prototype, not a production
  engine. Avoid premature architecture (no entity-component-system yet, no
  event bus yet — plain structs and functions are fine for MVP).

## How to proceed
Build the MVP in the order listed above (1 through 8), testing each piece
before moving to the next: get movement+jump working and confirmed by the
user, then stance switching, then fighter attack, then note input, then
chord detection. Ask before jumping ahead to a later step if the current
step hasn't been confirmed working.
