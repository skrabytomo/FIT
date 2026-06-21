#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  FIT — one-click build + launch
#  Double-click this file or run: bash launch.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$SCRIPT_DIR/game"
BUILD_DIR="$GAME_DIR/build"
BIN="$BUILD_DIR/bin/unnamed_strategy"

echo "========================================="
echo "  FIT Game Launcher"
echo "========================================="

# ── Step 1: Configure if build dir is missing ─────────────────────────────
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "[1/3] Configuring build system..."
    cmake -S "$GAME_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
else
    echo "[1/3] Build system already configured."
fi

# ── Step 2: Build ─────────────────────────────────────────────────────────
echo "[2/3] Building ($(nproc) threads)..."
cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1

if [ ! -f "$BIN" ]; then
    echo ""
    echo "ERROR: Build failed — executable not found at:"
    echo "  $BIN"
    exit 1
fi

echo "[3/3] Build successful."
echo ""

# ── Step 3: Launch ────────────────────────────────────────────────────────
echo "Launching game..."
cd "$GAME_DIR"
exec "$BIN"
