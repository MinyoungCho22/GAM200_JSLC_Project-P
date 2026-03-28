#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;
uniform vec2 texelSize;
uniform vec4 outlineColor;   

float alphaAt(vec2 uv)
{
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
            if (alphaAt(uv2) > 0.5)
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
    const float outlineWidthTexels = 2.0;
    const float fillAlpha          = 0.32;
    const float edgeBoost          = 0.55;
    const float scanlineDensity    = 320.0;
    const float scanlineStrength   = 0.18;
    const float outerGlowAlpha     = 0.10;

    float a = alphaAt(TexCoord);
    float r = clamp(outlineWidthTexels, 1.0, 3.0);

    bool inside    = (a > 0.1);
    bool edgeOuter = (a <= 0.1) && hasOpaqueNeighborInRadius(TexCoord, r);
    bool edgeInner = (a > 0.1)  && hasTransparentNeighborInRadius(TexCoord, r);

    if (!inside && !edgeOuter)
        discard;

    vec3 baseColor = outlineColor.rgb;

    float scan = sin(TexCoord.y * scanlineDensity) * 0.5 + 0.5;
    float scanMask = mix(1.0 - scanlineStrength, 1.0, scan);

    vec3 color = baseColor;
    float alpha = 0.0;

    if (inside)
    {
        color *= scanMask;

        if (edgeInner)
        {
            color *= (1.0 + edgeBoost);
        }

        alpha = fillAlpha * a;

        if (edgeInner)
        {
            alpha += 0.18;
        }
    }

    if (edgeOuter)
    {
        vec3 glowColor = baseColor * 1.15;
        float glowAlpha = outerGlowAlpha;

        color = max(color, glowColor);
        alpha = max(alpha, glowAlpha);
    }

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}