#!/bin/sh
# Fetches the unofficial iOS platform fork of raylib into vendor/raylib-ios
# (not tracked in git -- see .gitignore) and applies our local patches on
# top of it. Re-run this any time vendor/ is missing or needs refreshing.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR_DIR="$REPO_ROOT/vendor/raylib-ios"

rm -rf "$VENDOR_DIR"
git clone --depth 1 --branch features/ios-platform https://github.com/vsaint1/raylib.git "$VENDOR_DIR"
rm -rf "$VENDOR_DIR/.git"

for patch in "$REPO_ROOT"/ios/patches/*.patch; do
    echo "Applying $(basename "$patch")..."
    patch -p1 -d "$VENDOR_DIR" < "$patch"
done

echo "vendor/raylib-ios ready."
