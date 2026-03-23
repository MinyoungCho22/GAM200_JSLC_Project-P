// outline.frag
// Stable outline pass (outer + inner edge) with fixed thickness.
// This avoids missing edges when opaque pixels touch texture borders.

#version 330 core
out vec4 FragColor;
in  vec2 TexCoord;

uniform sampler2D ourTexture;
uniform vec2      texelSize;    // vec2(1.0/texWidth, 1.0/texHeight)
uniform vec4      outlineColor; // RGBA outline color
uniform float     outlineWidthTexels; // recommended 1.5 ~ 2.5

float alphaAt(vec2 uv)
{
    // Prevent clamp-to-edge artifacts: treat out-of-range UV as fully transparent.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 0.0;
    return texture(ourTexture, uv).a;
}

bool hasOpaqueNeighborInRadius(vec2 uv, float radiusTexel)
{
    const int MAX_RADIUS = 3;
    float r = clamp(radiusTexel, 1.0, float(MAX_RADIUS));
    float r2 = r * r;

    for (int y = -MAX_RADIUS; y <= MAX_RADIUS; ++y)
    {
        for (int x = -MAX_RADIUS; x <= MAX_RADIUS; ++x)
        {
            float d2 = float(x * x + y * y);
            if (d2 > r2) continue;

            vec2 uv2 = uv + vec2(float(x) * texelSize.x, float(y) * texelSize.y);
            if (alphaAt(uv2) > 0.5) // More stable threshold under linear filtering.
                return true;
        }
    }
    return false;
}

bool hasTransparentNeighborInRadius(vec2 uv, float radiusTexel)
{
    const int MAX_RADIUS = 3;
    float r = clamp(radiusTexel, 1.0, float(MAX_RADIUS));
    float r2 = r * r;

    for (int y = -MAX_RADIUS; y <= MAX_RADIUS; ++y)
    {
        for (int x = -MAX_RADIUS; x <= MAX_RADIUS; ++x)
        {
            float d2 = float(x * x + y * y);
            if (d2 > r2) continue;

            vec2 uv2 = uv + vec2(float(x) * texelSize.x, float(y) * texelSize.y);
            if (alphaAt(uv2) <= 0.1)
                return true;
        }
    }
    return false;
}

void main()
{
    float a = alphaAt(TexCoord);
    float r = clamp(outlineWidthTexels, 1.0, 3.0);

    // outer edge: transparent pixel near opaque
    bool edgeOuter = (a <= 0.1) && hasOpaqueNeighborInRadius(TexCoord, r);
    // inner edge: opaque pixel near transparent/outside
    bool edgeInner = (a >  0.1) && hasTransparentNeighborInRadius(TexCoord, r);

    if (edgeOuter || edgeInner) FragColor = outlineColor;
    else discard;
}
