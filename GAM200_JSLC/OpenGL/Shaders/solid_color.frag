//solid_color.frag

#version 330 core
out vec4 FragColor;

uniform vec3 objectColor;
uniform float uAlpha;

void main()
{
    FragColor = vec4(objectColor, uAlpha);
}