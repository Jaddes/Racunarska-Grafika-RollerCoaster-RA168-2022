# Tema 8 – Kandza (OpenGL)

Arcade claw machine built on the provided OpenGL 3.3 framework. Fullscreen, 75 FPS limited loop, textured cursor, glass box with two plush toys, active lamp, prize compartment, and state-driven claw logic.

## Requirements
- CMake 3.20+
- Windows: Visual Studio 2022 (MSVC) or MinGW-w64
- OpenGL 3.3 compatible GPU/driver
- GLFW, GLEW (already included in project sources)

## Build (Windows, MSVC)
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

## Run
```powershell
build/Debug/ClawMachine_Boris.exe
```

## Controls
- Left Click token slot: insert coin / start
- A / D: move claw horizontally
- W: raise claw (manual up)
- S: lower claw / drop toy
- Left Click prize: collect won toy
- ESC: exit

## Notes
- All assets are procedurally generated at runtime; no external textures required.
- The system cursor is hidden; the textured cursor is drawn in-scene.
