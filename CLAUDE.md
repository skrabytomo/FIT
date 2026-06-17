# Claude Code — Project Instructions

## Project overview
HoMM3-style hex-grid strategy game. C++20 / SDL2 / OpenGL 3.3 Core / ImGui 1.90.8.
See `game/GAME_PROJECT.md` for full design document, `game/HANDOFF.md` for architecture.

## Branch
All development goes on `claude/heroes-3-continue-t5wohf`. Never push to main/master.
Never create a pull request unless explicitly requested.

## Build
```bash
cd game && cmake --build build -j4
```
Executable: `game/build/bin/unnamed_strategy`

## Self-check before closing any task
1. Re-read every file you modified — confirm the change is present and correct
2. Run the build (`cmake --build build -j4`) and verify it succeeds with no new errors
3. For HUD/layout changes: trace screen coordinates by hand to confirm no overlap
4. For resource/economy changes: verify both the income source AND the spending sink
5. For bug fixes: identify the root cause in the code before writing the fix

## Key architecture rules
- `glClear` must use `GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT`
- `UIRenderer::endFrame()` must NOT call `m_textQueue.clear()` — `flushText()` does it
- World-map overlay labels and icons must be clipped against `HUD_TOP=68`, `HUD_BOTTOM=sh-52`, `HUD_RIGHT=sw-185`
- `CombatEngine::wait()` and board clicks must guard `WantCaptureMouse`
- ImGui popups: only one `BeginPopupModal` per frame — chain with `else if`
- Camera clamp: `limX = max(0, mapExtX - screenW/(2*zoom))` — viewport-compensated
- Default ImGui font has no Unicode — use ASCII only in all strings

## Resource economy rules
- Gold mines: `node.amount = 250` (not 3-5)
- Non-gold mines: `node.amount = 2-5`
- Mine income added each new week in `Game_WorldMap.cpp` after `m_turns.endTurn()`
- Building costs use `goldAndRes()` helper; shared buildings (Fort, Mage Guild) cost non-gold resources

## Repository scope
Only interact with `skrabytomo/fit`.
