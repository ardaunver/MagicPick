#!/bin/bash
# Builds and packages MagicPick.apk for Android from scratch: compiles the
# game's C++ sources against the prebuilt static libraylib.a (see
# setup_raylib.sh) into libmain.so, compiles the one small Java class
# (MainActivity.java -- a NativeActivity subclass that requests immersive/
# edge-to-edge mode, which only exists as a Java API) into classes.dex, then
# packages everything into an APK.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ANDROID_HOME="${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}"
NDK_VERSION="${NDK_VERSION:-26.3.11579264}"
BUILD_TOOLS_VERSION="${BUILD_TOOLS_VERSION:-34.0.0}"
API="${API:-29}"
TARGET_API="${TARGET_API:-34}"
ABI="${ABI:-arm64-v8a}"
JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home}"

NDK="$ANDROID_HOME/ndk/$NDK_VERSION"
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/darwin-x86_64"
BUILD_TOOLS="$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION"
PLATFORM_JAR="$ANDROID_HOME/platforms/android-$TARGET_API/android.jar"
RAYLIB_SRC="$ROOT/vendor/raylib-android/src"
CXX="$TOOLCHAIN/bin/aarch64-linux-android$API-clang++"
SYSROOT="$TOOLCHAIN/sysroot"

BUILD_DIR="$ROOT/build/android"
# Deliberately NOT inside BUILD_DIR, which gets wiped every run below --
# otherwise every build would silently generate a new signing key, and
# reinstalling over a previous build would fail with a signature mismatch.
KEYSTORE="$ROOT/build/android-keystore.jks"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/lib/$ABI" "$BUILD_DIR/res/values" "$BUILD_DIR/assets" "$BUILD_DIR/bin" "$BUILD_DIR/obj"

echo "== Compiling game sources into libmain.so =="
"$CXX" -shared -o "$BUILD_DIR/lib/$ABI/libmain.so" \
    "$ROOT/src/main.cpp" "$ROOT/src/player.cpp" "$ROOT/src/chord_system.cpp" \
    "$ROOT/src/enemy.cpp" "$ROOT/src/touch_controls.cpp" \
    -std=c++17 -DPLATFORM_ANDROID \
    -I"$RAYLIB_SRC" -I"$NDK/sources/android/native_app_glue" \
    --sysroot="$SYSROOT" -target aarch64-linux-android"$API" \
    -O2 -fPIC -Wl,-soname,libmain.so -u ANativeActivity_onCreate \
    -L"$RAYLIB_SRC" -lraylib -llog -landroid -lEGL -lGLESv2 -lOpenSLES -ldl -lm -lc

echo "== Copying libc++_shared.so (clang links it dynamically by default; must ship in the APK) =="
cp "$SYSROOT/usr/lib/aarch64-linux-android/libc++_shared.so" "$BUILD_DIR/lib/$ABI/libc++_shared.so"

echo "== Copying assets =="
cp -R "$ROOT/assets" "$BUILD_DIR/assets/assets"
find "$BUILD_DIR/assets" -name ".DS_Store" -delete

echo "== Generating manifest + resources =="
cp "$ROOT/android/AndroidManifest.xml.in" "$BUILD_DIR/AndroidManifest.xml"
cat > "$BUILD_DIR/res/values/strings.xml" << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<resources><string name="app_name">Magic Pick</string></resources>
XML

echo "== Compiling MainActivity.java (immersive/edge-to-edge mode) =="
"$JAVA_HOME/bin/javac" -d "$BUILD_DIR/obj" -classpath "$PLATFORM_JAR" \
    -source 11 -target 11 "$ROOT/android/MainActivity.java"
"$BUILD_TOOLS/d8" "$BUILD_DIR/obj/com/ardaunver/magicpick/MainActivity.class" \
    --release --output "$BUILD_DIR" --lib "$PLATFORM_JAR"

echo "== Packaging APK =="
"$BUILD_TOOLS/aapt" package -f -M "$BUILD_DIR/AndroidManifest.xml" -S "$BUILD_DIR/res" \
    -A "$BUILD_DIR/assets" -I "$PLATFORM_JAR" -F "$BUILD_DIR/bin/MagicPick.unaligned.apk" "$BUILD_DIR/bin"
(cd "$BUILD_DIR" && "$BUILD_TOOLS/aapt" add bin/MagicPick.unaligned.apk lib/$ABI/libmain.so lib/$ABI/libc++_shared.so classes.dex)

echo "== Zipaligning + signing =="
"$BUILD_TOOLS/zipalign" -p -f 4 "$BUILD_DIR/bin/MagicPick.unaligned.apk" "$BUILD_DIR/bin/MagicPick.aligned.apk"

if [ ! -f "$KEYSTORE" ]; then
    "$JAVA_HOME/bin/keytool" -genkeypair -validity 10000 -dname "CN=MagicPick,O=ardaunver,C=US" \
        -keystore "$KEYSTORE" -storepass magicpick -keypass magicpick -alias magicpickkey -keyalg RSA
fi
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-pass pass:magicpick --key-pass pass:magicpick \
    --ks-key-alias magicpickkey --out "$BUILD_DIR/MagicPick.apk" "$BUILD_DIR/bin/MagicPick.aligned.apk"

echo "== Done: $BUILD_DIR/MagicPick.apk =="
