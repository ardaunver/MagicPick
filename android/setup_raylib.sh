#!/bin/sh
# Fetches official raylib source (needed to build libraylib.a for Android --
# raylib's Android support is Makefile-only, no CMake path, unlike every
# other platform this project targets) into vendor/raylib-android (not
# tracked in git -- see .gitignore). Re-run any time vendor/ is missing.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR_DIR="$REPO_ROOT/vendor/raylib-android"

rm -rf "$VENDOR_DIR"
git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$VENDOR_DIR"
rm -rf "$VENDOR_DIR/.git"

echo "vendor/raylib-android ready."
