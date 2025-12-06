cmake_minimum_required(VERSION 3.15)
project(RollerCoaster_Boris)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RollerCoaster_Boris
    Source/Main.cpp
    Source/Util.cpp
    Header/Util.h
    Header/stb_image.h
)

target_include_directories(RollerCoaster_Boris PRIVATE Header)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(RollerCoaster_Boris PRIVATE OpenGL::GL glfw GLEW::GLEW)

file(COPY Source/Shaders DESTINATION ${CMAKE_BINARY_DIR}/Source)
