# Roller Coaster (OpenGL)

Minimal 2D/3D roller coaster scene with smooth Catmull-Rom track, segmented rails, and a silhouette background. Built with GLFW + GLEW on OpenGL 3.3.

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
build/Debug/RollerCoaster_Boris.exe
```

## Controls
- `SPACE` – add passenger
- `CLICK` – toggle belt / remove when returned
- `ENTER` – start ride
- `1-8` – make passenger sick during ride
- `ESC` – exit program

## Notes
- Track and visuals are self-contained; no external assets are required.
- If the window opens on a secondary monitor, ensure the monitor is primary or adjust the GLFW window creation to windowed mode in `Source/Main.cpp`.
