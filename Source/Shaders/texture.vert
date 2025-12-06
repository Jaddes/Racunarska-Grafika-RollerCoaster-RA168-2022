#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

uniform vec2 uPos;
uniform vec2 uSize;
uniform float uRotation;

void main()
{
    float c = cos(uRotation);
    float s = sin(uRotation);
    mat2 rot = mat2(c, -s, s, c);
    vec2 scaled = aPos * uSize;
    vec2 world = rot * scaled + uPos;
    vUV = aUV;
    gl_Position = vec4(world, 0.0, 1.0);
}
