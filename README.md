# Magic Pick

A 2D pixel-art action game. The player is a guitarist-sorcerer who switches
between a musician stance (playing chords to cast spells) and a fighter
stance (melee combat with the guitar as a weapon).

## Tech stack

- **Language:** C++17
- **Library:** [raylib](https://www.raylib.com/) 5.5
- **Build system:** CMake (desktop/iOS), a standalone shell script over the
  Android NDK directly (Android -- raylib has no CMake path for it)
- **Platforms:** macOS, Windows, iOS, Android (see below)

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
vendor/              iOS/Android raylib checkouts (fetched locally, not tracked in git)
build/               all build output, gitignored -- build/macos, build/ios,
                     build/android, build/windows per platform
```

## Building

All build output lives under `build/<platform>/`, gitignored in its
entirety -- nothing there needs to survive a fresh clone.

### macOS

Requires [raylib](https://www.raylib.com/) via Homebrew (`brew install raylib`).

```
cmake -B build/macos -S .
cmake --build build/macos
```

Produces a statically-linked `MagicPick` binary with no Homebrew runtime
dependency, portable to any Mac.

### Windows

Cross-compiled from macOS using `mingw-w64` and raylib's official Windows
binaries (matched to raylib 5.5), building into `build/windows`.

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
cmake -S . -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator
```

Open `build/ios/guitarist_sorcerer.xcodeproj` in Xcode, select the
**guitarist_sorcerer** scheme (not `ALL_BUILD`) with an iPhone Simulator
destination, and run. On first launch of a fresh Simulator, rotate the
device to landscape (Device menu → Rotate Left/Right, or Cmd+Left/Right) —
the app is landscape-locked, but the Simulator's own device orientation is
a separate setting that starts in portrait regardless.

**Current status:** builds and renders correctly on the iOS Simulator, with
touch controls (movement joystick, jump/attack buttons, chord wheel) wired
into gameplay (movement, jump, attack, spellcasting). Audio is disabled (the
vendored fork's audio backend doesn't compile for iOS yet).

### Android

Requires the Android SDK command-line tools, an NDK, and a JDK 17+:

```
brew install --cask android-commandlinetools
brew install openjdk@17
sdkmanager --sdk_root=/opt/homebrew/share/android-commandlinetools \
    "platform-tools" "platforms;android-34" "build-tools;34.0.0" \
    "ndk;26.3.11579264" "emulator" "system-images;android-34;google_apis;arm64-v8a"
```

Unlike every other platform here, raylib has no CMake support for Android at
all -- Android builds are Makefile-only upstream. So this target skips CMake
entirely: fetch raylib's source, build it into a static library with its own
Makefile, then compile this game's sources directly against it with the
NDK's own clang and package the result by hand (no Gradle, no Android
Studio project).

```
./android/run.sh
```

Handles everything end to end: fetches raylib into `vendor/raylib-android`
and creates the `MagicPickAVD` emulator on first run, boots the emulator if
one isn't already running, builds `build/android/MagicPick.apk`, installs
it, launches it, and streams the game's log output. Re-run any time after a
code change. `ANDROID_HOME`/`JAVA_HOME` default to the Homebrew paths above;
override them as environment variables if yours differ.

To build/install/launch as separate steps instead (e.g. against a real
device or a different emulator), see `android/build_apk.sh` -- `run.sh` is
a thin wrapper around it plus the emulator/adb bookkeeping.

The one bit of Java in this project (`android/MainActivity.java`, a thin
`NativeActivity` subclass) exists only to request immersive/edge-to-edge
mode -- something no C API exposes, and something raylib's Android backend
needs to see *before* it captures the screen size, or the game renders
letterboxed into the pre-immersive (smaller) area.

**Current status:** builds and runs correctly, full landscape scaling
(edge-to-edge, no letterboxing -- Android's official raylib backend needed
none of the custom rotation/compositor workarounds the iOS fork required),
with all touch controls (movement joystick, jump/attack buttons, chord
wheel, menu taps) verified working via `adb shell input tap/swipe`. Audio
works out of the box (raylib's Android audio backend compiles cleanly,
unlike the iOS fork's).

## Distribution

Packaged macOS and Windows builds (binary + assets, no build tools
required) are assembled under `dist/` for direct distribution.
