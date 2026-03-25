//simple.frag

#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

uniform float alpha;
uniform vec3 colorTint;
uniform float tintStrength;

void main()
{
    vec4 texColor = texture(ourTexture, TexCoord);
    vec3 tinted = mix(texColor.rgb, colorTint, tintStrength);
    FragColor = vec4(tinted, texColor.a * alpha);
}