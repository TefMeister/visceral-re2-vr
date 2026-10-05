#!/usr/bin/env bash
# Build visceral_native.dll (Release x64) and, with --deploy, install it + the bridge Lua into the Steam game.
#   bash dev-archive/native/tools/build.sh [--deploy]
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
GAME="/c/Steam/steamapps/common/RESIDENT EVIL 2  BIOHAZARD RE2"   # since 2026-10-04 we mod the Steam game
BUILD="$HERE/build"
[ -f "$BUILD/CMakeCache.txt" ] || cmake -S "$HERE" -B "$BUILD" -G "Visual Studio 17 2022" -A x64 >/dev/null
DLL="$BUILD/Release/visceral_native.dll"
rm -f "$DLL"   # a failed build must never leave an old DLL to deploy
cmake --build "$BUILD" --config Release 2>&1 | tee "$BUILD/last-build.log" | grep -E "warning|error|Build succeeded|FAILED" || true
[ -f "$DLL" ] || { echo "BUILD FAILED"; exit 1; }
ls -l "$DLL"; sha256sum "$DLL"
if [ "${1:-}" = "--deploy" ]; then
    mkdir -p "$GAME/reframework/plugins" "$GAME/reframework/autorun"
    cp "$DLL" "$GAME/reframework/plugins/visceral_native.dll"
    cp "$HERE/lua/visceral_bridge.lua" "$GAME/reframework/autorun/visceral_bridge.lua"
    cmp "$DLL" "$GAME/reframework/plugins/visceral_native.dll" && cmp "$HERE/lua/visceral_bridge.lua" "$GAME/reframework/autorun/visceral_bridge.lua" && echo "deployed OK"
fi
