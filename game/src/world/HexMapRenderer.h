#pragma once
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <string>
#include "HexMap.h"
#include "HexGrid.h"
#include "../renderer/Camera2D.h"
#include "../renderer/Shader.h"

// HexMapRenderer — draws hex tiles directly via OpenGL (no SpriteBatch needed)
// Uses a simple colored triangle-fan per hex
// Replaced by spritesheet tiles in Phase 1 — architecture stays the same

class HexMapRenderer
{
public:
    HexMapRenderer() = default;
    ~HexMapRenderer();

    bool init(float hexSize);
    void update(float dt) { m_time += dt; }
    void render(const HexMap& map, const Camera2D& camera,
                HexCoord hovered, HexCoord selected);

    const HexGrid& grid() const { return m_grid; }

private:
    void buildHexMesh();
    void drawHex(float cx, float cy, float r, float g, float b, float a, float scale = 1.0f);

    HexGrid m_grid{ 32.0f };
    Shader  m_shader;
    float   m_time  = 0.0f;

    GLuint  m_vao  = 0;
    GLuint  m_vbo  = 0;

    // Unit hex vertices (centered at origin, radius 1.0)
    // Scaled + translated per draw call via uniform
    static constexpr int HEX_VERTS = 8; // center + 6 corners + close
};
