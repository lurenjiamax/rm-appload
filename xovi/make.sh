#!/bin/bash
set -euo pipefail

TARGET_ARCH="${1:-${ARCH:-aarch64}}"
case "$TARGET_ARCH" in
    aarch64|arm32) ;;
    *) echo "Usage: $0 [aarch64|arm32]" >&2; exit 2 ;;
esac

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/xovi/build-$TARGET_ARCH"
QMAKE_BIN="${QMAKE:-qmake6}"
JOBS="${JOBS:-$(nproc)}"

if [ -z "${XOVI_REPO:-}" ]; then
    echo "XOVI_REPO must point to the XOVI checkout" >&2
    exit 1
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cp -r "$ROOT_DIR/src" "$BUILD_DIR/"
cp -r template/* "$BUILD_DIR/"
cp -r template/qmd "$BUILD_DIR/"

rcc --no-compress -g cpp "$ROOT_DIR/resources/resources.qrc" | sed '/#ifdef _MSC_VER/,/#endif/d' | sed -n '/#ifdef/q;p' > "$BUILD_DIR/resources.cpp"

cd "$BUILD_DIR"
python3 "$XOVI_REPO/util/xovigen.py" -o xovi.cpp -H xovi.h appload.xovi
"$QMAKE_BIN" CONFIG+=release .
make -j "$JOBS"
cp appload.so "$ROOT_DIR/xovi/appload-$TARGET_ARCH.so"
