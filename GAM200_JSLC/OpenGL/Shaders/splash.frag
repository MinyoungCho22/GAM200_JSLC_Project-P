// splash.frag  —  DigiPen "runner shadow" splash effect
//
// The logo is always bright/white. A narrow dark shadow band
// sweeps quickly from left to right, like a silhouette sprinting over it.
//
// C++ uniforms per frame:
//   uTime        – elapsed seconds (drives shadow position)
//   uAlpha       – global fade [0..1]
//   uBrightFloor – base brightness: ~2.0 during shadow phases, ~4.0 during bright hold
//   uShadowStr   – 1.0 = full dark shadow; 0.0 = no shadow (hold phase)

#version 330 core

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform float     uTime;
uniform float     uAlpha;
uniform vec2      uTexelSize;
uniform float     uBrightFloor;  // base logo brightness
uniform float     uShadowStr;    // 0 = no shadow, 1 = full shadow

// ── shadow parameters ─────────────────────────────────────────────────────
const float SHADOW_SPEED = 2.2;    // passes per second  (adjust feel here)
const float SHADOW_K     = 320.0;  // sharpness: higher = narrower shadow

// ── bloom ─────────────────────────────────────────────────────────────────
const float BLOOM_STR    = 0.55;

// Shadow multiplier: 0 at shadow center, 1 far away
float shadowMult(float x, float t)
{
    float pos = fract(t * SHADOW_SPEED);
    float d   = abs(x - pos);
    d         = min(d, 1.0 - d);                    // wrap around UV edge
    float dip = 1.0 - exp(-d * d * SHADOW_K);       // 0 at center, 1 elsewhere
    return mix(1.0, dip, uShadowStr);               // blend with shadow strength
}

// Brightness at a given x position
float brightAt(float x, float t)
{
    return uBrightFloor * shadowMult(x, t);
}

void main()
{
    vec4 base = texture(ourTexture, TexCoord);
    if (base.a < 0.01) discard;

    // ── Main colour ───────────────────────────────────────────────────────
    float br       = brightAt(TexCoord.x, uTime);
    vec3  mainColor = base.rgb * br;

    // ── Bloom ─────────────────────────────────────────────────────────────
    // Horizontal spread (neon/halation feel)
    vec3  bloom  = vec3(0.0);
    float totalW = 0.0;

    float hDist[4] = float[4](3.5, 8.0, 16.0, 28.0);
    for (int i = 0; i < 4; i++)
    {
        float wB = exp(-hDist[i] * 0.09);
        for (int s = -1; s <= 1; s += 2)
        {
            vec2  off  = vec2(float(s) * hDist[i] * uTexelSize.x, 0.0);
            vec4  samp = texture(ourTexture, TexCoord + off);
            float brN  = brightAt(TexCoord.x + off.x, uTime);
            bloom     += samp.rgb * brN * wB;
            totalW    += wB;
        }
    }

    // Vertical halo (tighter)
    float vDist[3] = float[3](2.5, 6.0, 11.0);
    for (int i = 0; i < 3; i++)
    {
        float wB = exp(-vDist[i] * 0.13);
        for (int s = -1; s <= 1; s += 2)
        {
            vec2  off  = vec2(0.0, float(s) * vDist[i] * uTexelSize.y);
            vec4  samp = texture(ourTexture, TexCoord + off);
            float brN  = brightAt(TexCoord.x, uTime);   // same x for vertical
            bloom     += samp.rgb * brN * wB;
            totalW    += wB;
        }
    }

    if (totalW > 0.001) bloom /= totalW;

    FragColor = vec4(mainColor + bloom * BLOOM_STR, base.a * uAlpha);
}
