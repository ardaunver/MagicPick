# Magic Pick

A 2D pixel-art action game. The player is a guitarist-sorcerer who switches
between a musician stance (playing chords to cast spells) and a fighter
stance (melee combat with the guitar as a weapon).

## Tech stack

- **Language:** C++17
- **Library:** [raylib](https://www.raylib.com/) 5.5
- **Build system:** CMake
- **Platforms:** macOS, Windows, iOS (see below)

All game art is either procedural (raylib primitives — shapes, gradients,
particles) or hand-authored pixel art loaded as textures. There is no game
engine or scripting layer; the game loop, rendering, and state machine are
plain C++ against raylib's API.

## Project structure

```
src/
  main.cpp          game loop, rendering, HUD, spell/combat logic
  player.h/.cpp      player state, movement, animation selection
  enemy.h/.cpp       enemy AI, combat, and rendering
  chord_system.h/.cpp  note input and chord-to-spell matching
assets/
  player/            sprite sheets, split by action (idle, attacks, jumps, casts)
  spells/            spell effect sprites
  enemies/           enemy sprites
  cave.png           background
vendor/              iOS-only raylib fork (fetched locally, not tracked in git)
```

## Building

### macOS

Requires [raylib](https://www.raylib.com/) via Homebrew (`brew install raylib`).

```
cmake -B build -S .
cmake --build build
```

Produces a statically-linked `MagicPick` binary with no Homebrew runtime
dependency, portable to any Mac.

### Windows

Cross-compiled from macOS using `mingw-w64` and raylib's official Windows
binaries (matched to raylib 5.5).

### iOS

Experimental. No official raylib release supports iOS yet, so the build
compiles raylib from source against an unofficial community fork
(`vsaint1/raylib`, branch `features/ios-platform`) rather than linking a
prebuilt package. First fetch the fork and apply this project's fixes to it
(see `ios/patches/`):

```
./ios/setup_vendor.sh
```

Then generate and build via CMake's Xcode generator:

```
cmake -S . -B build_ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator
```

Open `build_ios/guitarist_sorcerer.xcodeproj` in Xcode, select the
**guitarist_sorcerer** scheme (not `ALL_BUILD`) with an iPhone Simulator
destination, and run. On first launch of a fresh Simulator, rotate the
device to landscape (Device menu → Rotate Left/Right, or Cmd+Left/Right) —
the app is landscape-locked, but the Simulator's own device orientation is
a separate setting that starts in portrait regardless.

**Current status:** builds and renders correctly on the iOS Simulator.
Touch controls (movement joystick, jump/attack buttons, chord wheel) exist
and respond to touch, but aren't yet wired to player movement, combat, or
spellcasting. Audio is disabled (the vendored fork's audio backend doesn't
compile for iOS yet).

## Distribution

Packaged macOS and Windows builds (binary + assets, no build tools
required) are assembled under `dist/` for direct distribution.
