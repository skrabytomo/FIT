#include "HexMapRenderer.h"
#include <cmath>
#include <stdio.h>
#include <string.h>

static const char* s_vertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform mat4 uProj;
uniform vec2 uCenter;
uniform float uScale;
uniform vec4 uColor;

out vec4 vColor;

void main()
{
    vec2 world = aPos * uScale + uCenter;
    gl_Position = uProj * vec4(world, 0.0, 1.0);
    vColor = uColor;
}
)";

static const char* s_fragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
)";

// Terrain colors
static const float s_colors[][3] = {
    {0.71f, 0.78f, 0.45f}, // Plains
    {0.13f, 0.45f, 0.13f}, // Forest
    {0.55f, 0.50f, 0.35f}, // Highland
    {0.40f, 0.15f, 0.40f}, // Corrupted
    {0.30f, 0.45f, 0.20f}, // Toxic
    {0.95f, 0.95f, 0.70f}, // Sacred
    {0.45f, 0.45f, 0.50f}, // Industrial
    {0.60f, 0.55f, 0.45f}, // Rocky
    {0.25f, 0.35f, 0.20f}, // Swamp
    {0.20f, 0.40f, 0.70f}, // Water
    {0.70f, 0.25f, 0.10f}, // Volcanic
    {0.65f, 0.60f, 0.50f}, // Barren
    {0.35f, 0.30f, 0.25f}, // Wasteland
    {0.15f, 0.30f, 0.15f}, // CorruptedForest
    {0.60f, 0.30f, 0.30f}, // FleshZone
};

HexMapRenderer::~HexMapRenderer()
{
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool HexMapRenderer::init(float hexSize)
{
    m_grid = HexGrid(hexSize);

    if (!m_shader.loadFromSource(s_vertSrc, s_fragSrc))
        return false;

    buildHexMesh();
    printf("HexMapRenderer initialized (hex size %.0fpx)\n", hexSize);
    return true;
}

void HexMapRenderer::buildHexMesh()
{
    // Unit flat-top hex: center (0,0) + 6 corners at radius 1.0
    // Triangle fan: center, then 6 corners + wrap back to first corner
    // 8 vertices total
    float verts[HEX_VERTS * 2];
    verts[0] = 0.0f; verts[1] = 0.0f; // center

    for (int i = 0; i < 6; ++i) {
        float angle = (3.14159265f / 180.0f) * (60.0f * i);
        verts[(i + 1) * 2 + 0] = std::cos(angle);
        verts[(i + 1) * 2 + 1] = std::sin(angle);
    }
    // Close fan — repeat first corner
    verts[7 * 2 + 0] = verts[1 * 2 + 0];
    verts[7 * 2 + 1] = verts[1 * 2 + 1];

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
}

void HexMapRenderer::render(const HexMap& map, const Camera2D& camera,
                             HexCoord hovered, HexCoord selected)
{
    float proj[16];
    camera.getMatrix(proj);

    m_shader.bind();
    m_shader.setMat4("uProj", proj);
    glBindVertexArray(m_vao);

    for (auto& coord : map.coords()) {
        const HexTile* tile = map.getTile(coord);
        if (!tile || !tile->explored) continue;

        float cx, cy;
        m_grid.hexToWorld(coord, cx, cy);

        int ti = static_cast<int>(tile->terrain);
        float r = s_colors[ti][0];
        float g = s_colors[ti][1];
        float b = s_colors[ti][2];
        float a = tile->visible ? 1.0f : 0.55f;

        // Selected — gold outline (draw slightly larger first)
        if (coord == selected)
            drawHex(cx, cy, 1.0f, 0.85f, 0.0f, a, 1.06f);

        // Hovered — lighten
        if (coord == hovered) {
            r = std::min(1.0f, r + 0.15f);
            g = std::min(1.0f, g + 0.15f);
            b = std::min(1.0f, b + 0.15f);
        }

        drawHex(cx, cy, r, g, b, a, 1.0f);
    }

    glBindVertexArray(0);
    m_shader.unbind();
}

void HexMapRenderer::drawHex(float cx, float cy,
                              float r, float g, float b, float a, float scale)
{
    float s = m_grid.hexSize() * scale;
    m_shader.setVec2("uCenter", cx, cy);
    m_shader.setFloat("uScale", s);
    m_shader.setVec4("uColor", r, g, b, a);
    glDrawArrays(GL_TRIANGLE_FAN, 0, HEX_VERTS);
}
