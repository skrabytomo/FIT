#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  FIT — pull latest + rebuild (no launch)
#  Run this to update the game from the repo and rebuild.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$SCRIPT_DIR/game"
BUILD_DIR="$GAME_DIR/build"

echo "========================================="
echo "  FIT Game Updater"
echo "========================================="

# ── Pull latest changes ────────────────────────────────────────────────────
echo "[1/2] Pulling latest changes from git..."
cd "$SCRIPT_DIR"
git pull --ff-only

# ── Rebuild ────────────────────────────────────────────────────────────────
echo "[2/2] Rebuilding ($(nproc) threads)..."
cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1

echo ""
echo "Update complete. Run launch.sh to start the game."
