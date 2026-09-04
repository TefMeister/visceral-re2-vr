#!/usr/bin/env bash
# Build visceral_core.dll (Release, x64) with VS2022 Build Tools via CMake, then
# optionally deploy it into the game's reframework/plugins/ folder and verify the
# copy by hash. Run from anywhere:  bash tools/build.sh [--deploy]
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
GAME="/c/Steam/steamapps/common/RESIDENT EVIL 2  BIOHAZARD RE2"
BUILD="$HERE/build"

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    cmake -S "$HERE" -B "$BUILD" -G "Visual Studio 17 2022" -A x64
fi
cmake --build "$BUILD" --config Release 2>&1 | tee "$BUILD/last-build.log" | grep -E "warning|error|visceral_core\.(dll|vcxproj) ->|Build succeeded|FAILED" || true
DLL="$BUILD/Release/visceral_core.dll"
[ -f "$DLL" ] || { echo "BUILD FAILED: no $DLL"; exit 1; }
ls -l "$DLL"

# The two exports REFramework's loader looks for.
if command -v dumpbin >/dev/null 2>&1; then
    dumpbin //EXPORTS "$DLL" | grep -E "reframework_plugin_(initialize|required_version)" || echo "WARNING: exports missing"
fi

if [ "${1:-}" = "--deploy" ]; then
    mkdir -p "$GAME/reframework/plugins"
    if [ -f "$GAME/reframework/plugins/visceral_core.dll" ]; then
        cp "$GAME/reframework/plugins/visceral_core.dll" "$GAME/reframework/plugins/visceral_core.dll.prev"
    fi
    cp "$DLL" "$GAME/reframework/plugins/visceral_core.dll"
    cp "$HERE/../reframework/autorun/visceral_native_bridge.lua" "$GAME/reframework/autorun/visceral_native_bridge.lua"
    a=$(sha256sum "$DLL" | cut -c1-16); b=$(sha256sum "$GAME/reframework/plugins/visceral_core.dll" | cut -c1-16)
    [ "$a" = "$b" ] && echo "deployed OK sha256=$a" || { echo "DEPLOY HASH MISMATCH"; exit 1; }
fi
