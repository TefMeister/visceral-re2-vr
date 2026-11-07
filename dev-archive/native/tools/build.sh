#!/usr/bin/env bash
# Build visceral_native.dll (Release x64) and, with --deploy, put exactly one layer of the install list into the RE2 folder.
#   bash dev-archive/native/tools/build.sh [--deploy] [--layer N] [--dry]
# The clean rebuild (2026-11-07): install-list.txt says what each layer holds; deploy.py moves every other file of ours out
# (to D:/Visceral build versions/_removed-by-deploy/<time>/, never deleted). The default layer is the highest one listed.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DEPLOY=0; DRY=""; LAYER=""
while [ $# -gt 0 ]; do
    case "$1" in
        --deploy) DEPLOY=1 ;;
        --layer) LAYER="$2"; shift ;;
        --dry) DRY="--dry" ;;
        *) echo "unknown option $1"; exit 2 ;;
    esac
    shift
done
[ -n "$LAYER" ] || LAYER=$(grep -E '^[0-9]' "$HERE/tools/install-list.txt" | awk '{print $1}' | sort -n | tail -1)

if [ -f "$HERE/CMakeLists.txt" ]; then
    BUILD="$HERE/build"
    [ -f "$BUILD/CMakeCache.txt" ] || cmake -S "$HERE" -B "$BUILD" -G "Visual Studio 17 2022" -A x64 >/dev/null
    DLL="$BUILD/Release/visceral_native.dll"
    rm -f "$DLL"   # a failed build must never leave an old DLL to deploy
    cmake --build "$BUILD" --config Release 2>&1 | tee "$BUILD/last-build.log" | grep -E "warning|error|Build succeeded|FAILED" || true
    [ -f "$DLL" ] || { echo "BUILD FAILED"; exit 1; }
    ls -l "$DLL"; sha256sum "$DLL"
else
    echo "no plugin yet (layer 1 is the loader only)"
fi

if [ "$DEPLOY" = 1 ]; then
    python -I "$HERE/tools/deploy.py" --layer "$LAYER" $DRY
fi
