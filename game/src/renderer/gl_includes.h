#pragma once
// On Linux/Mesa, GL_GLEXT_PROTOTYPES exposes GL3.x function signatures from libGL.so
// without needing a separate loader like GLAD/GLEW.
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
