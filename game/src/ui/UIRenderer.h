#pragma once
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <string>
#include "UITypes.h"
#include "../renderer/Shader.h"

// UIRenderer — immediate-mode style, called each frame
// Draws filled rects, borders, simple text (text = placeholder until font system added)
class UIRenderer
{
public:
    bool init(int screenW, int screenH);
    void resize(int screenW, int screenH);

    // Call before any UI draw calls
    void beginFrame();
    void endFrame();

    // ── Primitives ─────────────────────────────────────────────────────────────
    void drawRect(const Rect& r, UIColor fill);
    void drawRect(const Rect& r, UIColor fill, UIColor border, float borderW = 1.0f);
    void drawRectRounded(const Rect& r, UIColor fill, float radius = 4.0f);

    // Bar (HP, mana, morale etc.)
    void drawBar(const Rect& r, float fraction,
                 UIColor fill, UIColor bg, UIColor border);

    // Text — placeholder colored rect until font system built
    // Will be replaced by font renderer in polish phase
    void drawText(const std::string& text, float x, float y,
                  UIColor color, float size = 14.0f);

    // Tooltip background
    void drawTooltip(const Rect& r);

    int screenW() const { return m_screenW; }
    int screenH() const { return m_screenH; }

    struct QuadVert { float x, y, r, g, b, a; };

private:
    void flushQuads();

    Shader  m_shader;
    GLuint  m_vao = 0, m_vbo = 0, m_ibo = 0;

    static constexpr int MAX_QUADS = 2048;
    QuadVert m_verts[MAX_QUADS * 4];
    int      m_quadCount = 0;

    float m_proj[16] = {};
    int   m_screenW  = 1280;
    int   m_screenH  = 720;
};
