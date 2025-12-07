#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Header/Util.h"

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
};

static float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

enum class GameState {
    Idle,
    ActiveNoToy,
    ActiveCarrying,
    ToyFalling,
    PrizeWaiting
};

const char* gameStateName(GameState s) {
    switch (s) {
    case GameState::Idle: return "Idle";
    case GameState::ActiveNoToy: return "ActiveNoToy";
    case GameState::ActiveCarrying: return "ActiveCarrying";
    case GameState::ToyFalling: return "ToyFalling";
    case GameState::PrizeWaiting: return "PrizeWaiting";
    default: return "Unknown";
    }
}

enum class LampMode {
    Off,
    Blue,
    Blink
};

struct Lamp {
    LampMode mode = LampMode::Off;
    float timer = 0.0f;
    bool blinkToggle = false;
    float interval = 0.5f;
};

struct Claw {
    Vec2 anchor{ 0.0f, 0.70f };  // Rope start (just above glass)
    float ropeLength = 0.16f;
    float minLength = 0.16f;
    float maxLength = 1.18f;
    float moveSpeed = 0.65f;
    float lowerSpeed = 0.80f;
    float raiseSpeed = 1.00f;
    float width = 0.12f;
    float height = 0.10f;
    bool open = false;
    bool movingDown = false;
    bool movingUp = false;
};

struct Toy {
    Vec2 pos{ 0.0f, 0.0f };
    Vec2 size{ 0.11f, 0.11f };
    Vec2 velocity{ 0.0f, 0.0f };
    unsigned int texture = 0;
    bool active = true;
    bool grabbed = false;
    bool falling = false;
    bool inPrize = false;
};

struct Hole {
    Vec2 center{ 0.48f, -0.28f };
    float radius = 0.085f;
};

struct PrizeCompartment {
    Vec2 pos{ 0.48f, -0.54f };
    Vec2 size{ 0.26f, 0.16f };
    bool hasToy = false;
    int toyIndex = -1;
};

struct TokenSlot {
    Vec2 pos{ -0.48f, -0.58f };
    Vec2 size{ 0.18f, 0.08f };
};

// Globals
GLFWwindow* window = nullptr;
int screenWidth = 1280;
int screenHeight = 720;
unsigned int colorShader = 0;
unsigned int textureShader = 0;
unsigned int quadVAO = 0;
unsigned int quadVBO = 0;

unsigned int toyTextureA = 0;
unsigned int toyTextureB = 0;
unsigned int toyTextureC = 0;
unsigned int holeTexture = 0;
unsigned int cursorTokenTex = 0;
unsigned int cursorLeverTex = 0;
unsigned int labelTex = 0;

GameState gameState = GameState::Idle;
Lamp lamp;
Claw claw;
Hole hole;
PrizeCompartment prize;
TokenSlot tokenSlot;
std::array<Toy, 4> toys;
std::array<Vec2, 6> spawnPositions = {
    Vec2{ -0.58f, -0.44f }, Vec2{ -0.32f, -0.44f }, Vec2{ -0.06f, -0.44f },
    Vec2{ 0.16f, -0.44f }, Vec2{ 0.36f, -0.44f }, Vec2{ 0.56f, -0.44f }
};
int nextSpawnSlot = 0;
int grabbedToyIndex = -1;
int fallingToyIndex = -1;
bool sWasDown = false;
Vec2 mouseGL{ 0.0f, 0.0f };
std::mt19937 rng(1337);
bool pendingPrizeClick = false;
float prizePulseTime = 0.0f;
int gClickCounter = 0;

// Bounds for the glass box
const Vec2 boxCenter{ 0.0f, 0.12f };
const Vec2 boxSize{ 1.26f, 1.06f };
const float boxLeft = boxCenter.x - boxSize.x * 0.5f;
const float boxRight = boxCenter.x + boxSize.x * 0.5f;
const float boxTop = boxCenter.y + boxSize.y * 0.5f;
const float boxBottom = boxCenter.y - boxSize.y * 0.5f;
const float floorY = boxBottom + 0.035f;
const float anchorStartY = boxTop + 0.08f;

// Forward decls
bool initGLFW();
bool initWindow();
bool initGLEW();
void initOpenGLState();
void createVAOs();
void mainLoop();
void update(float dt);
void render();
void windowToOpenGL(double mx, double my, float& glx, float& gly);
void mouseClickCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void drawQuadColor(const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& color);
void drawQuadTexture(unsigned int tex, const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& tint);

// Textures + helpers
std::vector<unsigned char> makeCircleTexture(int size, const std::array<unsigned char, 4>& fill);
std::vector<unsigned char> makeRingTexture(int size, const std::array<unsigned char, 4>& inner, const std::array<unsigned char, 4>& outer);
std::vector<unsigned char> makeToyTextureDots(int size);
std::vector<unsigned char> makeToyTextureStripes(int size);
std::vector<unsigned char> makeToyTextureChecks(int size);
std::vector<unsigned char> makeCoinTexture(int size);
std::vector<unsigned char> makeLeverTexture(int size);
std::unordered_map<char, std::array<uint8_t, 7>> fontGlyphs();
std::vector<unsigned char> makeLabelTexture(int width, int height, const std::string& text);

// Gameplay helpers
void resetMachine();
void spawnToys();
void startGame();
void startLowering();
void attachToy(int idx);
void releaseToy();
void collectPrize();
void updateLamp(float dt);
void updateClawMotion(float dt);
void updateFallingToy(float dt);
Vec2 clawPosition();
Vec2 clawGrabPoint();
bool pointInRect(const Vec2& p, const Vec2& center, const Vec2& size);
void configureLayout();

bool initGLFW()
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    return true;
}

bool initWindow()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    screenWidth = mode->width;
    screenHeight = mode->height;

    window = glfwCreateWindow(screenWidth, screenHeight, "CLAW MACHINE - Boris Lahos RA 168/2022", monitor, NULL);
    if (!window) return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    return true;
}

bool initGLEW()
{
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return false;
    return true;
}

void initOpenGLState() {
    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void windowToOpenGL(double mx, double my, float& glx, float& gly)
{
    glx = float((mx / screenWidth) * 2.0 - 1.0);
    gly = float(1.0 - (my / screenHeight) * 2.0);
}

void createVAOs()
{
    float quadVertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ---------------------- Texture generation helpers ---------------------- //
std::vector<unsigned char> makeCircleTexture(int size, const std::array<unsigned char, 4>& fill)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    Vec2 center = { size * 0.5f, size * 0.5f };
    float radius = size * 0.48f;
    float r2 = radius * radius;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - center.x;
            float dy = y - center.y;
            if (dx * dx + dy * dy <= r2) {
                int idx = (y * size + x) * 4;
                data[idx + 0] = fill[0];
                data[idx + 1] = fill[1];
                data[idx + 2] = fill[2];
                data[idx + 3] = fill[3];
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeRingTexture(int size, const std::array<unsigned char, 4>& inner, const std::array<unsigned char, 4>& outer)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    Vec2 c = { size * 0.5f, size * 0.5f };
    float outerR = size * 0.48f;
    float innerR = size * 0.26f;
    float o2 = outerR * outerR;
    float i2 = innerR * innerR;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - c.x;
            float dy = y - c.y;
            float d2 = dx * dx + dy * dy;
            int idx = (y * size + x) * 4;
            if (d2 <= i2) {
                data[idx + 0] = inner[0];
                data[idx + 1] = inner[1];
                data[idx + 2] = inner[2];
                data[idx + 3] = inner[3];
            }
            else if (d2 <= o2) {
                data[idx + 0] = outer[0];
                data[idx + 1] = outer[1];
                data[idx + 2] = outer[2];
                data[idx + 3] = outer[3];
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeToyTextureDots(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            data[idx + 0] = 190;
            data[idx + 1] = 110;
            data[idx + 2] = 200;
            data[idx + 3] = 255;
        }
    }
    for (int by = 0; by < size; by += 8) {
        for (int bx = 0; bx < size; bx += 8) {
            Vec2 center = { float(bx + 4), float(by + 4) };
            for (int y = by; y < by + 8 && y < size; ++y) {
                for (int x = bx; x < bx + 8 && x < size; ++x) {
                    float dx = x - center.x;
                    float dy = y - center.y;
                    if (dx * dx + dy * dy < 10.5f) {
                        int idx = (y * size + x) * 4;
                        data[idx + 0] = 250;
                        data[idx + 1] = 220;
                        data[idx + 2] = 120;
                        data[idx + 3] = 255;
                    }
                }
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeToyTextureStripes(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            bool bright = ((x / 6) % 2) == 0;
            data[idx + 0] = bright ? 90 : 60;
            data[idx + 1] = bright ? 170 : 120;
            data[idx + 2] = bright ? 230 : 180;
            data[idx + 3] = 255;
        }
    }
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (((y / 6) % 2) == 0) {
                int idx = (y * size + x) * 4;
                data[idx + 0] = std::min<int>(255, data[idx + 0] + 30);
                data[idx + 1] = std::min<int>(255, data[idx + 1] + 30);
                data[idx + 2] = std::min<int>(255, data[idx + 2] + 10);
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeToyTextureChecks(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    std::array<unsigned char, 4> c1 = { 70, 170, 220, 255 };
    std::array<unsigned char, 4> c2 = { 35, 120, 180, 255 };
    int block = 6;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool alt = ((x / block) + (y / block)) % 2 == 0;
            const auto& c = alt ? c1 : c2;
            int idx = (y * size + x) * 4;
            data[idx + 0] = c[0];
            data[idx + 1] = c[1];
            data[idx + 2] = c[2];
            data[idx + 3] = c[3];
        }
    }
    return data;
}

std::vector<unsigned char> makeCoinTexture(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    Vec2 c{ size * 0.5f, size * 0.5f };
    float r = size * 0.45f;
    float r2 = r * r;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - c.x;
            float dy = y - c.y;
            float d2 = dx * dx + dy * dy;
            if (d2 <= r2) {
                float shade = 0.75f + 0.25f * (dy / r);
                shade = std::clamp(shade, 0.6f, 1.0f);
                int idx = (y * size + x) * 4;
                data[idx + 0] = static_cast<unsigned char>(230 * shade);
                data[idx + 1] = static_cast<unsigned char>(190 * shade);
                data[idx + 2] = static_cast<unsigned char>(70 * shade);
                data[idx + 3] = 255;
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeLeverTexture(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    auto setPix = [&](int x, int y, const std::array<unsigned char, 4>& c) {
        if (x < 0 || x >= size || y < 0 || y >= size) return;
        int idx = (y * size + x) * 4;
        data[idx + 0] = c[0];
        data[idx + 1] = c[1];
        data[idx + 2] = c[2];
        data[idx + 3] = c[3];
    };
    std::array<unsigned char, 4> body = { 200, 200, 210, 255 };
    std::array<unsigned char, 4> grip = { 90, 120, 230, 255 };

    for (int y = 4; y < size - 4; ++y) {
        for (int x = 6; x < 14; ++x) {
            setPix(x, y, body);
        }
    }
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 20; ++x) {
            if (x + y < 16) setPix(x, y, grip);
        }
    }
    return data;
}

std::unordered_map<char, std::array<uint8_t, 7>> fontGlyphs()
{
    return {
        { 'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
        { 'B',{0b11110,0b10001,0b11110,0b10001,0b10001,0b10001,0b11110} },
        { 'C',{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110} },
        { 'D',{0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100} },
        { 'E',{0b11111,0b10000,0b11100,0b10000,0b10000,0b10000,0b11111} },
        { 'F',{0b11111,0b10000,0b11100,0b10000,0b10000,0b10000,0b10000} },
        { 'G',{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110} },
        { 'H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
        { 'I',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111} },
        { 'J',{0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100} },
        { 'K',{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001} },
        { 'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111} },
        { 'M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001} },
        { 'N',{0b10001,0b11001,0b10101,0b10101,0b10011,0b10001,0b10001} },
        { 'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
        { 'P',{0b11110,0b10001,0b11110,0b10000,0b10000,0b10000,0b10000} },
        { 'Q',{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101} },
        { 'R',{0b11110,0b10001,0b11110,0b10001,0b10001,0b10001,0b10001} },
        { 'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110} },
        { 'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100} },
        { 'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
        { 'V',{0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100} },
        { 'W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001} },
        { 'X',{0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001} },
        { 'Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100} },
        { 'Z',{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111} },
        { ' ',{0,0,0,0,0,0,0} },
        { '/',{0b00001,0b00010,0b00100,0b01000,0b10000,0,0} },
        { '0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110} },
        { '1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110} },
        { '2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111} },
        { '3',{0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110} },
        { '4',{0b10010,0b10010,0b10010,0b11111,0b00010,0b00010,0b00010} },
        { '5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110} },
        { '6',{0b01110,0b10000,0b11110,0b10001,0b10001,0b10001,0b01110} },
        { '7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000} },
        { '8',{0b01110,0b10001,0b01110,0b10001,0b10001,0b10001,0b01110} },
        { '9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b11110} },
        { '-',{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000} },
        { ':',{0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000} },
    };
}

std::vector<unsigned char> makeLabelTexture(int width, int height, const std::string& text)
{
    std::vector<unsigned char> data(width * height * 4, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            data[idx + 0] = 20;
            data[idx + 1] = 24;
            data[idx + 2] = 32;
            data[idx + 3] = 180;
        }
    }

    auto glyphs = fontGlyphs();
    int scale = 2;
    int lineHeight = 7 * scale + 6;
    int marginX = 16;
    int cursorX = marginX;
    int cursorY = 28;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
        if (c == '\n') {
            cursorX = marginX;
            cursorY += lineHeight;
            continue;
        }
        if (glyphs.find(c) == glyphs.end()) continue;
        const auto& rows = glyphs[c];
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col))) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = cursorX + col * scale + sx;
                            int py = cursorY + row * scale + sy;
                            if (px >= 0 && px < width && py >= 0 && py < height) {
                                int idx = (py * width + px) * 4;
                                data[idx + 0] = 235;
                                data[idx + 1] = 235;
                                data[idx + 2] = 245;
                                data[idx + 3] = 255;
                            }
                        }
                    }
                }
            }
        }
        cursorX += 6 * scale;
    }
    return data;
}

// ---------------------- Gameplay helpers ---------------------- //
void resetMachine()
{
    configureLayout();

    gameState = GameState::Idle;
    lamp.mode = LampMode::Off;
    lamp.timer = 0.0f;
    lamp.blinkToggle = false;

    claw.anchor = { 0.0f, anchorStartY };
    claw.ropeLength = claw.minLength;
    claw.open = false;
    claw.movingDown = false;
    claw.movingUp = false;

    prize.hasToy = false;
    prize.toyIndex = -1;
    grabbedToyIndex = -1;
    fallingToyIndex = -1;
    sWasDown = false;
}

void spawnToys()
{
    std::uniform_int_distribution<int> slotDist(0, static_cast<int>(spawnPositions.size()) - 1);
    int startSlot = slotDist(rng);
    std::array<unsigned int, 3> toyTextures = { toyTextureA, toyTextureB, toyTextureC };
    for (size_t i = 0; i < toys.size(); ++i) {
        Vec2 spawn = spawnPositions[(startSlot + static_cast<int>(i)) % spawnPositions.size()];
        toys[i].size = { 0.11f, 0.11f };
        spawn.y = floorY + toys[i].size.y * 0.5f;
        toys[i].pos = spawn;
        toys[i].velocity = { 0.0f, 0.0f };
        toys[i].grabbed = false;
        toys[i].falling = false;
        toys[i].inPrize = false;
        toys[i].active = true;
        toys[i].texture = toyTextures[i % toyTextures.size()];
    }
    nextSpawnSlot = (startSlot + static_cast<int>(toys.size())) % spawnPositions.size();
}

void startGame()
{
    if (gameState != GameState::Idle) return;
    lamp.mode = LampMode::Blue;
    claw.open = true;
    gameState = GameState::ActiveNoToy;
}

void startLowering()
{
    if (claw.movingDown || claw.movingUp) return;
    claw.movingDown = true;
}

void attachToy(int idx)
{
    grabbedToyIndex = idx;
    toys[idx].grabbed = true;
    toys[idx].falling = false;
    toys[idx].velocity = { 0.0f, 0.0f };
    claw.open = false;
    gameState = GameState::ActiveCarrying;
    claw.movingDown = false;
    claw.movingUp = true;
}

void releaseToy()
{
    if (grabbedToyIndex < 0) return;
    Toy& t = toys[grabbedToyIndex];
    t.grabbed = false;
    t.falling = true;
    t.velocity = { 0.0f, 0.0f };
    t.pos = clawGrabPoint();
    fallingToyIndex = grabbedToyIndex;
    grabbedToyIndex = -1;
    claw.open = true;
    gameState = GameState::ToyFalling;
}

void collectPrize()
{
    std::cout << "[COLLECT] collectPrize() called. prize.hasToy=" << (prize.hasToy ? 1 : 0)
        << " toyIndex=" << prize.toyIndex
        << " stateBefore=" << gameStateName(gameState) << std::endl;

    if (!prize.hasToy || prize.toyIndex < 0) return;
    int idx = prize.toyIndex;
    toys[idx].inPrize = false;
    toys[idx].falling = false;
    toys[idx].grabbed = false;
    toys[idx].active = true;
    // Respawn toy to keep the machine playable.
    Vec2 respawn = spawnPositions[nextSpawnSlot];
    respawn.y = floorY + toys[idx].size.y * 0.5f;
    toys[idx].pos = respawn;
    nextSpawnSlot = (nextSpawnSlot + 1) % spawnPositions.size();

    prize.hasToy = false;
    prize.toyIndex = -1;
    pendingPrizeClick = false;
    resetMachine();
    lamp.mode = LampMode::Off;
    claw.open = false;

    std::cout << "[COLLECT] DONE: prize cleared, stateAfter=" << gameStateName(gameState)
        << " prize.hasToy=" << (prize.hasToy ? 1 : 0) << std::endl;
}

void updateLamp(float dt)
{
    if (lamp.mode == LampMode::Blink) {
        lamp.timer += dt;
        if (lamp.timer >= lamp.interval) {
            lamp.timer = 0.0f;
            lamp.blinkToggle = !lamp.blinkToggle;
        }
    }
}

Vec2 clawPosition()
{
    return { claw.anchor.x, claw.anchor.y - claw.ropeLength };
}

Vec2 clawGrabPoint()
{
    Vec2 pos = clawPosition();
    return { pos.x, pos.y - claw.height * 0.35f };
}

bool pointInRect(const Vec2& p, const Vec2& center, const Vec2& size)
{
    return std::abs(p.x - center.x) <= size.x * 0.5f && std::abs(p.y - center.y) <= size.y * 0.5f;
}

bool pointInPrizeArea(const Vec2& p)
{
    // Enlarge clickable area to match visual glow and reduce miss clicks.
    return pointInRect(p, prize.pos, prize.size * 1.40f);
}

void configureLayout()
{
    hole.center = { boxRight - 0.20f, floorY + 0.11f };
    hole.radius = 0.085f;

    prize.pos = { boxRight - 0.10f, boxBottom - 0.16f };
    prize.size = { 0.32f, 0.16f };

    tokenSlot.pos = { boxLeft + 0.30f, boxBottom - 0.14f };
    tokenSlot.size = { 0.22f, 0.08f };

    claw.anchor = { 0.0f, anchorStartY };
    claw.ropeLength = claw.minLength;
}

void updateClawMotion(float dt)
{
    // Horizontal movement
    if (gameState == GameState::ActiveNoToy || gameState == GameState::ActiveCarrying || gameState == GameState::ToyFalling) {
        float moveDir = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += 1.0f;
        claw.anchor.x += moveDir * claw.moveSpeed * dt;
        claw.anchor.x = std::clamp(claw.anchor.x, boxLeft + 0.10f, boxRight - 0.10f);
    }

    if (claw.movingDown) {
        claw.ropeLength += claw.lowerSpeed * dt;
        claw.ropeLength = std::min(claw.ropeLength, claw.maxLength);
        Vec2 cPos = clawPosition();
        if (cPos.y - claw.height * 0.5f <= floorY) {
            claw.movingDown = false;
            claw.movingUp = true;
        }
        for (int i = 0; i < static_cast<int>(toys.size()); ++i) {
            Toy& t = toys[i];
            if (!t.active || t.falling || t.inPrize) continue;
            float dx = std::abs(cPos.x - t.pos.x);
            float dy = std::abs(cPos.y - t.pos.y);
            if (dx <= (claw.width * 0.5f + t.size.x * 0.35f) && dy <= (claw.height * 0.5f + t.size.y * 0.35f)) {
                attachToy(i);
                break;
            }
        }
    }
    if (claw.movingUp) {
        claw.ropeLength -= claw.raiseSpeed * dt;
        if (claw.ropeLength <= claw.minLength) {
            claw.ropeLength = claw.minLength;
            claw.movingUp = false;
        }
    }
}

void updateFallingToy(float dt)
{
    if (fallingToyIndex < 0) return;
    Toy& t = toys[fallingToyIndex];
    if (!t.falling) {
        fallingToyIndex = -1;
        return;
    }
    const float gravity = -2.6f;
    t.velocity.y += gravity * dt;
    t.pos.y += t.velocity.y * dt;

    // Hole detection
    float holeDist = length({ t.pos.x - hole.center.x, t.pos.y - hole.center.y });
    if (holeDist < hole.radius * 0.75f && t.pos.y <= hole.center.y + 0.02f) {
        t.falling = false;
        t.inPrize = true;
        prize.hasToy = true;
        prize.toyIndex = fallingToyIndex;
        t.pos = prize.pos;
        lamp.mode = LampMode::Blink;
        lamp.timer = 0.0f;
        gameState = GameState::PrizeWaiting;
        claw.open = false;
        claw.movingDown = false;
        claw.movingUp = true;

        std::cout << "[HOLE] Toy entered hole -> prize.hasToy=1, state=" << gameStateName(gameState)
            << ", toyIndex=" << prize.toyIndex << std::endl;

        fallingToyIndex = -1;
        return;
    }

    // Floor hit
    float minY = floorY + t.size.y * 0.5f;
    if (t.pos.y <= minY) {
        t.pos.y = minY;
        t.velocity = { 0.0f, 0.0f };
        t.falling = false;
        fallingToyIndex = -1;
        gameState = GameState::ActiveNoToy;
    }
}

void mouseClickCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    gClickCounter++;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    windowToOpenGL(mx, my, mouseGL.x, mouseGL.y);
    std::cout << "\n[CLICK #" << gClickCounter << "] mouseGL=(" << mouseGL.x << ", " << mouseGL.y << ")"
        << " state=" << gameStateName(gameState)
        << " prize.hasToy=" << (prize.hasToy ? 1 : 0)
        << " prize.toyIndex=" << prize.toyIndex
        << std::endl;

    // Prize collection: if clicked slightly early while toy is on the way, remember the intent.
    if (prize.hasToy && pointInPrizeArea(mouseGL)) {
        // Always collect immediately when a prize exists; no state gating to avoid timing misses.
        std::cout << "[CLICK #" << gClickCounter << "] HIT: prize area, prize.hasToy=1 -> collectPrize()" << std::endl;
        collectPrize();
        return;
    }

    if (pointInRect(mouseGL, tokenSlot.pos, tokenSlot.size)) {
        std::cout << "[CLICK #" << gClickCounter << "] HIT: token slot, state=" << gameStateName(gameState) << std::endl;
        if (gameState == GameState::Idle) {
            std::cout << "[CLICK #" << gClickCounter << "] ACTION: startGame()" << std::endl;
            startGame();
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, true);
    }
}

void update(float dt)
{
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    windowToOpenGL(mx, my, mouseGL.x, mouseGL.y);

    updateLamp(dt);
    updateClawMotion(dt);

    bool sDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool wDown = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    if (sDown && !sWasDown) {
        if (gameState == GameState::ActiveNoToy && !claw.movingDown && !claw.movingUp) startLowering();
        else if (gameState == GameState::ActiveCarrying && !claw.movingDown && !claw.movingUp) releaseToy();
    }
    // Manual vertical control when not auto-raising and gameplay is active.
    bool allowManual = (gameState == GameState::ActiveNoToy || gameState == GameState::ActiveCarrying) && !claw.movingUp && !claw.movingDown;
    if (allowManual) {
        if (sDown) {
            claw.ropeLength = std::min(claw.ropeLength + claw.lowerSpeed * dt, claw.maxLength);
        }
        if (wDown) {
            claw.ropeLength = std::max(claw.ropeLength - claw.raiseSpeed * dt, claw.minLength);
        }
    }
    sWasDown = sDown;

    // Keep grabbed toy attached
    if (grabbedToyIndex >= 0) {
        toys[grabbedToyIndex].pos = clawGrabPoint();
    }

    updateFallingToy(dt);

    // Auto-honor a pending prize click once state is ready.
    if (pendingPrizeClick && prize.hasToy && gameState == GameState::PrizeWaiting) {
        collectPrize();
        pendingPrizeClick = false;
    }

    // Advance prize pulse timer for visual cue.
    if (prize.hasToy) prizePulseTime += dt; else prizePulseTime = 0.0f;
}

// ---------------------- Rendering ---------------------- //
void drawQuadColor(const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& color)
{
    glUseProgram(colorShader);
    glUniform2f(glGetUniformLocation(colorShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(colorShader, "uSize"), size.x, size.y);
    glUniform1f(glGetUniformLocation(colorShader, "uRotation"), rot);
    glUniform4f(glGetUniformLocation(colorShader, "uColor"), color[0], color[1], color[2], color[3]);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void drawQuadTexture(unsigned int tex, const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& tint)
{
    glUseProgram(textureShader);
    glUniform2f(glGetUniformLocation(textureShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(textureShader, "uSize"), size.x, size.y);
    glUniform1f(glGetUniformLocation(textureShader, "uRotation"), rot);
    glUniform4f(glGetUniformLocation(textureShader, "uTint"), tint[0], tint[1], tint[2], tint[3]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(textureShader, "uTex"), 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void renderBackground()
{
    drawQuadColor({ 0.0f, 0.0f }, { 2.4f, 2.4f }, 0.0f, { 0.05f, 0.06f, 0.08f, 1.0f });
}

void renderCabinet()
{
    std::array<float, 4> cyan = { 0.15f, 0.68f, 0.74f, 1.0f };
    std::array<float, 4> darkBlue = { 0.10f, 0.24f, 0.34f, 1.0f };
    std::array<float, 4> brown = { 0.38f, 0.27f, 0.17f, 1.0f };

    Vec2 cabinetCenter = { 0.0f, boxCenter.y - 0.03f };
    Vec2 cabinetSize = { boxSize.x + 0.24f, boxSize.y + 0.36f };
    drawQuadColor(cabinetCenter, cabinetSize, 0.0f, cyan);

    // Side trims
    float trimWidth = 0.08f;
    drawQuadColor({ boxLeft - trimWidth * 0.5f, boxCenter.y }, { trimWidth, boxSize.y + 0.32f }, 0.0f, brown);
    drawQuadColor({ boxRight + trimWidth * 0.5f, boxCenter.y }, { trimWidth, boxSize.y + 0.32f }, 0.0f, brown);

    // Top cover
    drawQuadColor({ cabinetCenter.x, boxTop + 0.16f }, { cabinetSize.x, 0.18f }, 0.0f, darkBlue);
    drawQuadColor({ cabinetCenter.x, boxTop + 0.24f }, { cabinetSize.x, 0.04f }, 0.0f, brown);

    // Bottom control area
    drawQuadColor({ cabinetCenter.x, boxBottom - 0.18f }, { cabinetSize.x, 0.26f }, 0.0f, darkBlue);
    drawQuadColor({ cabinetCenter.x, boxBottom - 0.28f }, { cabinetSize.x, 0.06f }, 0.0f, brown);
}

void renderGlassBox()
{
    // Glass tint
    drawQuadColor(boxCenter, boxSize, 0.0f, { 0.75f, 0.95f, 0.98f, 0.20f });
    // Inner overlay to darken a bit
    drawQuadColor(boxCenter, boxSize, 0.0f, { 0.08f, 0.10f, 0.12f, 0.18f });

    // Top band inside glass
    drawQuadColor({ boxCenter.x, boxTop - 0.04f }, { boxSize.x, 0.04f }, 0.0f, { 0.12f,0.20f,0.28f,0.45f });
    // Floor strip
    drawQuadColor({ boxCenter.x, floorY - 0.01f }, { boxSize.x, 0.04f }, 0.0f, { 0.06f,0.08f,0.10f,0.35f });
}

void renderHole()
{
    drawQuadTexture(holeTexture, hole.center, { hole.radius * 2.0f, hole.radius * 2.0f }, 0.0f, { 0.9f,0.9f,0.95f,0.85f });
}

void renderPrizeCompartment()
{
    drawQuadColor(prize.pos, prize.size, 0.0f, { 0.12f,0.20f,0.28f,1.0f });
    drawQuadColor(prize.pos + Vec2{ 0.0f, prize.size.y * 0.20f }, { prize.size.x * 1.05f, 0.02f }, 0.0f, { 0.40f,0.50f,0.55f,1.0f });
    if (prize.hasToy) {
        float pulse = 0.45f + 0.35f * std::sin(prizePulseTime * 6.0f);
        std::array<float, 4> glow = { 0.95f, 0.95f, 0.35f, pulse };
        drawQuadColor(prize.pos, prize.size * 1.15f, 0.0f, glow);
    }
    if (prize.hasToy && prize.toyIndex >= 0) {
        drawQuadTexture(toys[prize.toyIndex].texture, prize.pos, toys[prize.toyIndex].size * 1.1f, 0.0f, { 1.0f,1.0f,1.0f,1.0f });
    }
}

void renderTokenSlot()
{
    drawQuadColor(tokenSlot.pos, tokenSlot.size, 0.0f, { 0.38f,0.27f,0.17f,1.0f });
    drawQuadColor(tokenSlot.pos + Vec2{ 0.0f, 0.01f }, { tokenSlot.size.x * 0.75f, 0.012f }, 0.0f, { 0.96f,0.80f,0.32f,1.0f });
}

void renderLamp()
{
    std::array<float, 4> off = { 0.15f,0.15f,0.15f,1.0f };
    std::array<float, 4> blue = { 0.2f,0.5f,1.0f,1.0f };
    std::array<float, 4> green = { 0.1f,0.9f,0.3f,1.0f };
    std::array<float, 4> red = { 0.95f,0.1f,0.1f,1.0f };
    std::array<float, 4> color = off;
    if (lamp.mode == LampMode::Blue) color = blue;
    else if (lamp.mode == LampMode::Blink) color = lamp.blinkToggle ? green : red;

    Vec2 lampPos = { boxCenter.x, boxTop + 0.18f };
    drawQuadColor(lampPos, { 0.16f, 0.10f }, 0.0f, { 0.08f,0.08f,0.10f,1.0f });
    drawQuadColor(lampPos, { 0.12f, 0.08f }, 0.0f, color);
}

void renderRopeAndClaw()
{
    Vec2 cPos = clawPosition();
    Vec2 ropeCenter = { claw.anchor.x, (claw.anchor.y + cPos.y) * 0.5f };
    float ropeLen = claw.anchor.y - cPos.y;
    std::array<float, 4> rail = { 0.18f,0.45f,0.75f,1.0f };
    std::array<float, 4> joint = { 0.10f,0.24f,0.34f,1.0f };
    float railY = boxTop - 0.04f;
    drawQuadColor({ (boxLeft + boxRight) * 0.5f, railY }, { boxSize.x, 0.03f }, 0.0f, rail);
    drawQuadColor({ claw.anchor.x, railY - 0.04f }, { 0.08f, 0.08f }, 0.0f, joint);

    drawQuadColor(ropeCenter, { 0.012f, ropeLen }, 0.0f, { 0.85f,0.85f,0.90f,1.0f });

    std::array<float, 4> clawColor = claw.open ? std::array<float, 4>{ 0.90f,0.92f,0.96f,1.0f } : std::array<float, 4>{ 0.64f,0.66f,0.72f,1.0f };
    std::array<float, 4> clawShadow = { 0.08f,0.10f,0.12f,0.35f };
    drawQuadColor(cPos + Vec2{ 0.01f, -0.01f }, { claw.width, claw.height }, 0.0f, clawShadow);
    drawQuadColor(cPos, { claw.width, claw.height }, 0.0f, clawColor);
    // Small jaws
    float jawOffset = claw.width * 0.25f;
    float jawWidth = claw.width * 0.18f;
    float jawHeight = claw.height * 0.6f;
    if (claw.open) {
        drawQuadColor(cPos + Vec2{ -jawOffset, -jawHeight * 0.25f }, { jawWidth, jawHeight }, 0.35f, clawColor);
        drawQuadColor(cPos + Vec2{ jawOffset, -jawHeight * 0.25f }, { jawWidth, jawHeight }, -0.35f, clawColor);
    }
    else {
        drawQuadColor(cPos + Vec2{ -jawOffset * 0.6f, -jawHeight * 0.2f }, { jawWidth, jawHeight }, 0.05f, clawColor);
        drawQuadColor(cPos + Vec2{ jawOffset * 0.6f, -jawHeight * 0.2f }, { jawWidth, jawHeight }, -0.05f, clawColor);
    }
}

void renderToys()
{
    for (const auto& t : toys) {
        if (!t.active || t.inPrize) continue;
        drawQuadTexture(t.texture, t.pos, t.size, 0.0f, { 1.0f,1.0f,1.0f,1.0f });
    }
}

void renderLabel()
{
    drawQuadTexture(labelTex, { 0.0f, 0.82f }, { 1.6f, 0.28f }, 0.0f, { 1.0f,1.0f,1.0f,1.0f });
}

void renderCursor()
{
    unsigned int tex = (gameState == GameState::Idle) ? cursorTokenTex : cursorLeverTex;
    Vec2 size = (gameState == GameState::Idle) ? Vec2{ 0.08f, 0.08f } : Vec2{ 0.10f, 0.10f };
    Vec2 pos = mouseGL;
    if (tex == cursorLeverTex) {
        pos = mouseGL + Vec2{ size.x * 0.5f, size.y * 0.5f };
    }
    drawQuadTexture(tex, pos, size, 0.0f, { 1.0f,1.0f,1.0f,1.0f });
}

void render()
{
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderBackground();
    renderCabinet();
    renderGlassBox();
    renderPrizeCompartment();
    renderTokenSlot();
    renderHole();
    renderToys();
    renderRopeAndClaw();
    renderLamp();
    renderLabel();
    renderCursor();
}

// ---------------------- Main loop ---------------------- //
void mainLoop()
{
    const double targetFrame = 1.0 / 75.0;
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        update(dt);
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        double frameTime = glfwGetTime() - now;
        if (frameTime < targetFrame) {
            std::this_thread::sleep_for(std::chrono::duration<double>(targetFrame - frameTime));
        }
    }
}

int main()
{
    if (!initGLFW()) return endProgram("GLFW init failed.");
    if (!initWindow()) return endProgram("Window creation failed.");
    if (!initGLEW()) return endProgram("GLEW init failed.");

    glfwSetMouseButtonCallback(window, mouseClickCallback);
    glfwSetKeyCallback(window, keyCallback);

    createVAOs();

    colorShader = createShader("Source/Shaders/color.vert", "Source/Shaders/color.frag");
    textureShader = createShader("Source/Shaders/texture.vert", "Source/Shaders/texture.frag");

    toyTextureA = createTextureFromRGBA(makeToyTextureDots(64), 64, 64);
    toyTextureB = createTextureFromRGBA(makeToyTextureStripes(64), 64, 64);
    toyTextureC = createTextureFromRGBA(makeToyTextureChecks(64), 64, 64);
    holeTexture = createTextureFromRGBA(makeRingTexture(96, { 20,25,32,210 }, { 80,90,110,190 }), 96, 96);
    cursorTokenTex = createTextureFromRGBA(makeCoinTexture(64), 64, 64);
    cursorLeverTex = createTextureFromRGBA(makeLeverTexture(64), 64, 64);
    labelTex = createTextureFromRGBA(
        makeLabelTexture(1024, 220,
            "BORIS LAHOS RA 168/2022\n\n"
            "LEFT CLICK TOKEN SLOT  - START GAME\n"
            "A / D                  - MOVE CLAW\n"
            "W                      - RAISE CLAW (manual up)\n"
            "S                      - LOWER / DROP\n"
            "LEFT CLICK PRIZE       - COLLECT TOY\n"
            "ESC                    - EXIT"),
        1024, 220);

    spawnToys();
    resetMachine();
    initOpenGLState();
    mainLoop();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
