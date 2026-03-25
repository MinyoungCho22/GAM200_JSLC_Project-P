// silhouette.frag
// Renders a sprite as a pure black silhouette using only the texture's alpha channel.

#version 330 core

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float     alpha;        // global transparency

void main()
{
    float a = texture(ourTexture, TexCoord).a;
    if (a < 0.08) discard;
    FragColor = vec4(0.0, 0.0, 0.0, a * alpha);
}
