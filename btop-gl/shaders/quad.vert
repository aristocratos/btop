#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec2 FragPos;

uniform mat4 projection;
uniform vec2 offset;
uniform vec2 scale;

void main() {
    vec2 position = aPos * scale + offset;
    gl_Position = projection * vec4(position, 0.0, 1.0);
    TexCoord = aTexCoord;
    FragPos = position;
} 