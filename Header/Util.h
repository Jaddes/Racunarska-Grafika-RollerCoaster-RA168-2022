#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

int endProgram(const std::string& message);
unsigned int createShader(const char* vsSource, const char* fsSource);
unsigned int loadImageToTexture(const char* filePath);
unsigned int createTextureFromRGBA(const std::vector<unsigned char>& data, int width, int height);
GLFWcursor* loadImageToCursor(const char* filePath);
