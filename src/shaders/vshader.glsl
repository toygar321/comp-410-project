#version 460

layout (location = 0) in vec2 vPos;

out vec2 TexCoords;

void main()
{
    TexCoords = vPos * 0.5 + 0.5;
    gl_Position = vec4(vPos, 0.0, 1.0);
}