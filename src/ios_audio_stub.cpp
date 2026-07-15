// Temporary no-op audio stand-ins for iOS. raudio.c can't currently be
// compiled for this platform (miniaudio's AVFoundation backend needs
// Objective-C compilation, which the vendored iOS fork doesn't do yet), so
// the audio module is excluded from the build entirely (see
// SUPPORT_MODULE_RAUDIO in CMakeLists.txt) and these stand in for the few
// raylib audio calls the game makes, so the game builds and runs silently
// instead of failing to link. Real iOS audio is a separate, later task.
#include "raylib.h"

void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}

Sound LoadSoundFromWave(Wave wave) {
    (void)wave;
    Sound sound = {};
    return sound;
}

void UnloadSound(Sound sound) {
    (void)sound;
}

void PlaySound(Sound sound) {
    (void)sound;
}
