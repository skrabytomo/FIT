#include "HexMapRenderer.h"
#include <cmath>
#include <stdio.h>
#include <string.h>

static const char* s_vertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform mat4  uProj;
uniform vec2  uCenter;
uniform float uScale;
uniform vec4  uColor;

out vec4 vColor;
out vec2 vWorldPos;
out vec2 vTexCoord;

void main()
{
    vec2 world   = aPos * uScale + uCenter;
    gl_Position  = uProj * vec4(world, 0.0, 1.0);
    vColor       = uColor;
    vWorldPos    = world;
    vTexCoord    = aPos * 0.5 + 0.5;
}
)";

static const char* s_fragSrc = R"(
#version 330 core
in vec4 vColor;
in vec2 vWorldPos;
in vec2 vTexCoord;

uniform int        uTerrain;
uniform float      uTime;
uniform sampler2D  uTerrainTex;
uniform int        uUseTexture;

out vec4 fragColor;

float hash(vec2 p) {
    p = fract(p * vec2(234.56, 789.01));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    if (uTerrain < 0) { fragColor = vColor; return; }

    vec2 wp = vWorldPos * 0.022;
    float pat = 0.0;

    if (uTerrain == 0) {
        pat = (vnoise(wp * 4.0) - 0.5) * 0.14;
    } else if (uTerrain == 1) {
        float n = vnoise(wp * 2.8);
        pat = (n > 0.55) ? -0.22 : 0.04;
    } else if (uTerrain == 2) {
        float s = fract((wp.x + wp.y * 0.6) * 4.5);
        pat = (s < 0.35) ? 0.14 : -0.06;
    } else if (uTerrain == 3) {
        float n = vnoise(wp * 2.2);
        float n2 = vnoise(wp * 5.5 + 3.1);
        pat = (n > 0.52) ? -0.28 + n2 * 0.08 : 0.04;
    } else if (uTerrain == 4) {
        vec2 cell = floor(wp * 5.5);
        vec2 fc   = fract(wp * 5.5) - 0.5;
        float r   = hash(cell) * 0.28 + 0.12;
        pat = (length(fc) < r) ? 0.18 : -0.05;
    } else if (uTerrain == 5) {
        vec2 lp  = fract(wp * 2.0) - 0.5;
        float ang = atan(lp.y, lp.x);
        float sp  = abs(fract(ang * 4.0 / 3.14159 + 0.5) - 0.5);
        pat = (sp < 0.08) ? 0.20 : 0.01;
    } else if (uTerrain == 6) {
        vec2 g = fract(wp * 5.0);
        pat = (g.x < 0.07 || g.y < 0.07) ? -0.20 : 0.06;
    } else if (uTerrain == 7) {
        float n1 = vnoise(wp * 4.5);
        float n2 = vnoise(wp * 9.0 + 3.7);
        pat = (abs(n1 - 0.5) < 0.04) ? -0.24 : (n2 - 0.5) * 0.10;
    } else if (uTerrain == 8) {
        float w = sin(wp.y * 7.0 + wp.x * 1.8) * 0.5 + 0.5;
        pat = (w > 0.68) ? -0.14 : (w < 0.32) ? 0.09 : 0.0;
    } else if (uTerrain == 9) {
        float w = sin(wp.x * 5.5 - uTime * 1.6) * sin(wp.y * 3.8 + uTime * 0.9);
        pat = w * 0.14;
    } else if (uTerrain == 10) {
        float n = vnoise(wp * 3.0);
        pat = (abs(n - 0.5) < 0.06) ? 0.28 : -0.10;
    } else if (uTerrain == 11) {
        vec2 cell = floor(wp * 4.5);
        vec2 fc   = fract(wp * 4.5) - 0.5;
        float r   = hash(cell) * 0.18;
        pat = (hash(cell) > 0.55 && length(fc) < r) ? -0.22 : 0.0;
    } else if (uTerrain == 12) {
        float n  = vnoise(wp * 5.0);
        float n2 = vnoise(wp * 11.0 + 1.3);
        pat = ((n > 0.5) ? n2 : -n2) * 0.16;
    } else if (uTerrain == 13) {
        float n  = vnoise(wp * 2.0);
        float n2 = vnoise(wp * 5.0 + 2.1);
        pat = (n > 0.48) ? -0.28 + n2 * 0.09 : n2 * 0.06;
    } else {
        float v = abs(sin(wp.x * 3.2 + sin(wp.y * 4.1) * 2.0));
        pat = (v < 0.14) ? 0.22 : -0.04;
    }

    vec3 col;
    if (uUseTexture != 0) {
        // Animated UV scroll for water
        vec2 uv = vTexCoord;
        if (uTerrain == 9) uv.x += uTime * 0.03;

        vec3 tex = texture(uTerrainTex, uv).rgb;
        col = clamp(tex + pat * 0.25, 0.0, 1.0);
        col = col * vColor.rgb;  // vColor.rgb is brightness tint (hover = 1.25)
    } else {
        col = clamp(vColor.rgb + pat, 0.0, 1.0);
    }
    fragColor = vec4(col, vColor.a);
}
)";

// Fallback terrain colors (used when texture not loaded)
static const float s_colors[][3] = {
    {0.38f, 0.43f, 0.22f}, // Plains
    {0.08f, 0.28f, 0.10f}, // Forest
    {0.44f, 0.39f, 0.28f}, // Highland
    {0.28f, 0.10f, 0.30f}, // Corrupted
    {0.28f, 0.38f, 0.08f}, // Toxic
    {0.72f, 0.68f, 0.42f}, // Sacred
    {0.30f, 0.30f, 0.36f}, // Industrial
    {0.48f, 0.43f, 0.34f}, // Rocky
    {0.17f, 0.24f, 0.11f}, // Swamp
    {0.07f, 0.20f, 0.52f}, // Water
    {0.52f, 0.11f, 0.04f}, // Volcanic
    {0.50f, 0.40f, 0.25f}, // Barren
    {0.28f, 0.25f, 0.20f}, // Wasteland
    {0.10f, 0.20f, 0.09f}, // CorruptedForest
    {0.52f, 0.26f, 0.26f}, // FleshZone
};

static const char* s_terrainFiles[15] = {
    "assets/terrain/plains.png",
    "assets/terrain/forest.png",
    "assets/terrain/highland.png",
    "assets/terrain/corrupted.png",
    "assets/terrain/toxic.png",
    "assets/terrain/sacred.png",
    "assets/terrain/industrial.png",
    "assets/terrain/rocky.png",
    "assets/terrain/swamp.png",
    "assets/terrain/water.png",
    "assets/terrain/volcanic.png",
    "assets/terrain/barren.png",
    "assets/terrain/wasteland.png",
    "assets/terrain/corrupted_forest.png",
    "assets/terrain/flesh_zone.png",
};

HexMapRenderer::~HexMapRenderer()
{
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

bool HexMapRenderer::init(float hexSize, const std::string& basePath)
{
    m_grid = HexGrid(hexSize);

    if (!m_shader.loadFromSource(s_vertSrc, s_fragSrc))
        return false;

    buildHexMesh();

    // Set sampler to slot 0 once
    m_shader.bind();
    m_shader.setInt("uTerrainTex", 0);
    m_shader.unbind();

    // Load terrain textures (non-fatal if missing — falls back to procedural)
    int loaded = 0;
    for (int i = 0; i < NUM_TERRAIN; ++i) {
        // Try basePath-prefixed path first, then bare relative path as fallback
        std::string path = basePath + s_terrainFiles[i];
        if (!m_terrainTex[i].load(path, false, false)) {
            if (!basePath.empty())
                m_terrainTex[i].load(s_terrainFiles[i], false, false);
        }
        if (m_terrainTex[i].ok()) loaded++;
    }
    printf("HexMapRenderer: %d/%d terrain textures loaded (hex size %.0fpx)\n",
           loaded, NUM_TERRAIN, hexSize);
    return true;
}

void HexMapRenderer::buildHexMesh()
{
    float verts[HEX_VERTS * 2];
    verts[0] = 0.0f; verts[1] = 0.0f;

    for (int i = 0; i < 6; ++i) {
        float angle = (3.14159265f / 180.0f) * (60.0f * i);
        verts[(i + 1) * 2 + 0] = std::cos(angle);
        verts[(i + 1) * 2 + 1] = std::sin(angle);
    }
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
    m_shader.setFloat("uTime", m_time);
    glBindVertexArray(m_vao);

    for (auto& coord : map.coords()) {
        const HexTile* tile = map.getTile(coord);
        if (!tile || !tile->explored) continue;

        float cx, cy;
        m_grid.hexToWorld(coord, cx, cy);

        int ti = static_cast<int>(tile->terrain);
        float a = tile->visible ? 1.0f : 0.55f;

        bool hasTexture = m_terrainTex[ti].ok();

        // Selected — gold outline ring
        if (coord == selected) {
            m_shader.setInt("uTerrain", -1);
            m_shader.setInt("uUseTexture", 0);
            drawHex(cx, cy, 1.0f, 0.85f, 0.0f, a, 1.06f);
        }

        float r, g, b;
        if (hasTexture) {
            m_terrainTex[ti].bind(0);
            m_shader.setInt("uUseTexture", 1);
            // vColor.rgb = brightness tint: 1.0 normal, 1.25 hovered
            float bright = (coord == hovered) ? 1.25f : 1.0f;
            r = bright; g = bright; b = bright;
        } else {
            m_shader.setInt("uUseTexture", 0);
            r = s_colors[ti][0];
            g = s_colors[ti][1];
            b = s_colors[ti][2];
            if (coord == hovered) {
                r = std::min(1.0f, r + 0.15f);
                g = std::min(1.0f, g + 0.15f);
                b = std::min(1.0f, b + 0.15f);
            }
        }

        m_shader.setInt("uTerrain", ti);
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
