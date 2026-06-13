#pragma once
#include <algorithm>
#include <imgui.h>

// ── Animation state ───────────────────────────────────────────────────────────
enum class AnimState : uint8_t { Idle, Attack, Hurt, Dead };

// ── Per-unit sprite animator ──────────────────────────────────────────────────
// Atlas layout (384×384 per faction):
//   8 cols = frames: [idle:4][attack:2][hurt:1][dead:1]
//   6 rows = tiers 1-6

struct SpriteAnimator
{
    AnimState state    = AnimState::Idle;
    float     t        = 0.0f;
    int       frame    = 0;    // 0-3 within current state
    int       faction  = 0;    // 0-8, indexes into m_spriteAtlas[]
    int       tier     = 1;    // 1-6, selects atlas row
    bool      mirror   = false; // enemy units face left

    // Column offsets in the 8-frame atlas row
    static constexpr int COL_IDLE   = 0;  // cols 0-3
    static constexpr int COL_ATTACK = 4;  // cols 4-5
    static constexpr int COL_HURT   = 6;  // col  6
    static constexpr int COL_DEAD   = 7;  // col  7
    static constexpr int TOTAL_COLS = 8;
    static constexpr int TOTAL_ROWS = 6;

    void setState(AnimState s)
    {
        if (s == state) return;
        state = s; frame = 0; t = 0.0f;
    }

    void update(float dt)
    {
        int   nf  = numFrames();
        float dur = frameDuration();
        t += dt;
        if (t >= dur) {
            t -= dur;
            frame = (frame + 1) % nf;
            // Non-looping states snap back to Idle when done
            if (frame == 0 && (state == AnimState::Attack || state == AnimState::Hurt))
                state = AnimState::Idle;
        }
    }

    // Compute UV corners for the current frame.
    // u0/u1 are already mirrored if mirror==true.
    void getUV(float& u0, float& v0, float& u1, float& v1) const
    {
        int col = atlasCol();
        int row = std::max(0, tier - 1);

        float fw = 1.0f / TOTAL_COLS;
        float fh = 1.0f / TOTAL_ROWS;

        float pu0 = col * fw;
        float pu1 = pu0 + fw;
        v0 = row * fh;
        v1 = v0  + fh;

        if (mirror) { u0 = pu1; u1 = pu0; }
        else        { u0 = pu0; u1 = pu1; }
    }

private:
    int numFrames() const
    {
        switch (state) {
        case AnimState::Idle:   return 4;
        case AnimState::Attack: return 2;
        default:                return 1;  // Hurt, Dead
        }
    }

    float frameDuration() const
    {
        switch (state) {
        case AnimState::Idle:   return 0.22f;
        case AnimState::Attack: return 0.10f;
        case AnimState::Hurt:   return 0.12f;
        default:                return 1.0f;  // Dead: hold indefinitely
        }
    }

    int atlasCol() const
    {
        switch (state) {
        case AnimState::Idle:   return COL_IDLE   + frame;
        case AnimState::Attack: return COL_ATTACK + frame;
        case AnimState::Hurt:   return COL_HURT;
        default:                return COL_DEAD;
        }
    }
};
