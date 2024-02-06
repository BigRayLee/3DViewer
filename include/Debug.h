#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

using namespace std;

/*gl error check based on glGetError */
#define glCheckError() glCheckError_(__FILE__, __LINE__)
GLenum glCheckError_(const char *file, int line);

/*gl conext debug
*/
void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar *message, void *useParam);
