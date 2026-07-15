#!/bin/bash
# One-command Android dev loop: boots the emulator if one isn't already
# running, fetches raylib and builds the APK if needed, installs it, and
# launches the game. Re-run this after any code change instead of doing the
# setup/adb steps by hand.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export ANDROID_HOME="${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}"
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home}"
export PATH="$ANDROID_HOME/platform-tools:$ANDROID_HOME/emulator:$PATH"

AVD_NAME="${AVD_NAME:-MagicPickAVD}"
PACKAGE="com.ardaunver.magicpick"

if [ ! -d "$ROOT/vendor/raylib-android" ]; then
    echo "== Fetching raylib source (first run only) =="
    "$ROOT/android/setup_raylib.sh"
fi

if [ -z "$(adb devices | grep -w device)" ]; then
    if [ -z "$(avdmanager list avd | grep "$AVD_NAME")" ]; then
        echo "== Creating $AVD_NAME (first run only) =="
        echo "no" | avdmanager create avd -n "$AVD_NAME" \
            -k "system-images;android-34;google_apis;arm64-v8a" -d pixel_7 --force
        CONFIG="$HOME/.android/avd/$AVD_NAME.avd/config.ini"
        if ! grep -q "hw.initialOrientation" "$CONFIG"; then
            echo "hw.initialOrientation=landscape" >> "$CONFIG"
        fi
    fi

    echo "== No device/emulator running, booting $AVD_NAME =="
    nohup emulator -avd "$AVD_NAME" -no-audio > /dev/null 2>&1 &
    disown
    adb wait-for-device
    echo "== Waiting for boot to complete =="
    adb shell 'while [[ -z $(getprop sys.boot_completed) ]]; do sleep 1; done;'
fi

echo "== Building APK =="
"$ROOT/android/build_apk.sh"

echo "== Installing =="
adb install -r "$ROOT/build/android/MagicPick.apk"

echo "== Launching =="
adb shell am start -n "$PACKAGE/.MainActivity"

echo "== Done. Streaming logs (Ctrl+C to stop watching -- the game keeps running) =="
adb logcat -c
adb logcat raylib:V AndroidRuntime:E *:S
