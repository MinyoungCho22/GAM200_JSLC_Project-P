#version 330 core

layout(location = 0) in vec2 aPos;   

out vec2 vUV;

void main()
{
    vUV = aPos;

    vec2 clip = aPos * 2.0 - 1.0;

    gl_Position = vec4(clip, 0.0, 1.0);
}
