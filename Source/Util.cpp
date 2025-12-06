#include "../Header/Util.h"

#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include <sstream>
#include <iostream>
#include <array>
#include <filesystem>
#include <windows.h>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "../Header/stb_image.h"

int endProgram(const std::string& message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

// Try to resolve asset paths regardless of the working directory.
static std::string resolveAssetPath(const std::string& relative)
{
    namespace fs = std::filesystem;
    // First, trust the incoming path (works when launched from the build dir).
    fs::path candidate = relative;
    if (fs::exists(candidate)) return candidate.string();

    // Then, look relative to the executable location and its parent (build/).
    char exePathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        fs::path exeDir = fs::path(exePathBuf).parent_path();
        std::array<fs::path, 2> bases = { exeDir, exeDir.parent_path() };
        for (const auto& base : bases) {
            if (base.empty()) continue;
            candidate = base / relative;
            if (fs::exists(candidate)) return candidate.string();
        }
    }

    // Fall back to the original string if nothing was found.
    return relative;
}

unsigned int compileShader(GLenum type, const char* source)
{
    std::string resolvedPath = resolveAssetPath(source);
    std::string content = "";
    std::ifstream file(resolvedPath);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Read shader file: \"" << resolvedPath << "\"\n";
    }
    else {
        ss << "";
        std::cout << "Failed to read shader file: \"" << resolvedPath << "\"\n";
    }
    std::string temp = ss.str();
    const char* sourceCode = temp.c_str();

    int shader = glCreateShader(type);

    int success;
    char infoLog[512];
    glShaderSource(shader, 1, &sourceCode, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << " shader error:\n";
        std::cout << infoLog << std::endl;
    }
    return shader;
}
unsigned int createShader(const char* vsSource, const char* fsSource)
{
    unsigned int program = glCreateProgram();
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vsSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    glValidateProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "Program validation failed:\n" << infoLog << std::endl;
    }

    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}

unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);
    if (ImageData != NULL)
    {
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        std::cout << "Texture not loaded! Path: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return 0;
    }
}

unsigned int createTextureFromRGBA(const std::vector<unsigned char>& data, int width, int height) {
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // Flip vertically so text and other UI textures render upright.
    std::vector<unsigned char> flipped(data.size());
    int rowSize = width * 4;
    for (int y = 0; y < height; ++y) {
        int src = y * rowSize;
        int dst = (height - 1 - y) * rowSize;
        std::memcpy(flipped.data() + dst, data.data() + src, rowSize);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLFWcursor* loadImageToCursor(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;

    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);

    if (ImageData != NULL)
    {
        GLFWimage image;
        image.width = TextureWidth;
        image.height = TextureHeight;
        image.pixels = ImageData;

        int hotspotX = TextureWidth / 5;
        int hotspotY = TextureHeight / 5;

        GLFWcursor* cursor = glfwCreateCursor(&image, hotspotX, hotspotY);
        stbi_image_free(ImageData);
        return cursor;
    }
    else {
        std::cout << "Cursor image not loaded! Path: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return nullptr;
    }
}
