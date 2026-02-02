#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTex;
uniform float uExposure;

void main()
{
    vec3 color = texture(uSceneTex, vUV).rgb;
    color *= uExposure;
    FragColor = vec4(color, 1.0);
}
