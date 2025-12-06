#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec4 uTint;

void main()
{
    vec4 tex = texture(uTex, vUV);
    FragColor = tex * uTint;
}
