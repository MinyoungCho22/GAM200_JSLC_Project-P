#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;
uniform vec2 texelSize;
uniform vec4 outlineColor;   
uniform bool radialScanline;

uniform float uTime;
uniform bool isFireGlow;
uniform bool isPulseVent;

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

    float threshold = isPulseVent ? 0.2 : 0.5;

    for (int y = -MAX_RADIUS; y <= MAX_RADIUS; ++y)
    {
        for (int x = -MAX_RADIUS; x <= MAX_RADIUS; ++x)
        {
            float d2 = float(x * x + y * y);
            if (d2 > r2) continue;

            vec2 uv2 = uv + vec2(float(x) * texelSize.x, float(y) * texelSize.y);
            if (isPulseVent && (uv2.x < 0.13 || uv2.x > 0.86))
                continue;

            if (alphaAt(uv2) > threshold)
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

    vec2 distortedUV = TexCoord;
    if (isFireGlow)
    {
        // rising heat distortion wave
        distortedUV.x += sin(TexCoord.y * 15.0 - uTime * 8.0) * 0.006;
        distortedUV.y += cos(TexCoord.x * 15.0 - uTime * 8.0) * 0.003;
    }

    float a = alphaAt(distortedUV);
    if (isPulseVent)
    {
        if (distortedUV.x < 0.13 || distortedUV.x > 0.86)
        {
            a = 0.0;
        }
        else
        {
            // Scan left and right from the current X coordinate to find boundary lines of Pulse_Vent.png
            bool hitLeft = false;
            float stepX = max(texelSize.x, 0.002);
            for (float x = distortedUV.x - stepX; x >= 0.0; x -= stepX)
            {
                if (x < 0.13) break;
                if (alphaAt(vec2(x, distortedUV.y)) > 0.2)
                {
                    hitLeft = true;
                    break;
                }
            }
            if (hitLeft)
            {
                bool hitRight = false;
                for (float x = distortedUV.x + stepX; x <= 1.0; x += stepX)
                {
                    if (x > 0.86) break;
                    if (alphaAt(vec2(x, distortedUV.y)) > 0.2)
                    {
                        hitRight = true;
                        break;
                    }
                }
                if (hitRight && distortedUV.y <= 0.95)
                {
                    a = max(a, 1.0);
                }
            }
        }
    }
    float r = clamp(outlineWidthTexels, 1.0, 3.0);

    if (isFireGlow)
    {
        // modulate outline width to simulate flickering flame peaks
        float pulse = sin(uTime * 14.0 + TexCoord.y * 30.0) * 0.35 + 0.65;
        r = clamp(outlineWidthTexels * pulse, 1.0, 3.0);
    }

    bool inside    = (a > 0.1);
    bool edgeOuter = (a <= 0.1) && hasOpaqueNeighborInRadius(distortedUV, r);
    bool edgeInner = (a > 0.1)  && hasTransparentNeighborInRadius(distortedUV, r);

    if (!inside && !edgeOuter)
        discard;

    vec3 baseColor = outlineColor.rgb;
    if (isFireGlow)
    {
        // fiery color gradient: orange (1.0, 0.35, 0.0) to yellow (1.0, 0.85, 0.1)
        float colorFactor = sin(uTime * 12.0 + TexCoord.y * 25.0) * 0.5 + 0.5;
        baseColor = mix(vec3(1.0, 0.35, 0.0), vec3(1.0, 0.85, 0.1), colorFactor);
    }

    float scan = 1.0;
    float currentStrength = scanlineStrength;
    if (radialScanline)
    {
        vec2 toTip = TexCoord - vec2(0.5, 0.0);
        float angle = atan(toTip.y, toTip.x);
        scan = sin(angle * 80.0) * 0.5 + 0.5;
        currentStrength = 0.75;
    }
    else
    {
        scan = sin(TexCoord.y * scanlineDensity) * 0.5 + 0.5;
    }
    float scanMask = mix(1.0 - currentStrength, 1.0, scan);

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
        if (isPulseVent)
        {
            alpha = max(alpha, 0.45);
        }
        if (radialScanline)
        {
            alpha = max(alpha, 0.35);
        }

        if (isFireGlow)
        {
            alpha = 0.15 * a; // keep inside mostly transparent
        }

        if (edgeInner)
        {
            alpha += 0.18;
            if (isFireGlow)
            {
                alpha += 0.25; // boost inner edge for fire highlights
            }
        }
    }

    if (edgeOuter)
    {
        vec3 glowColor = baseColor * 1.15;
        float glowAlpha = outerGlowAlpha;
        if (isFireGlow)
        {
            // stronger flickering outer glow for fire
            float pulse = sin(uTime * 15.0 + TexCoord.y * 10.0) * 0.4 + 0.6;
            glowAlpha = 0.35 * pulse;
        }

        color = max(color, glowColor);
        alpha = max(alpha, glowAlpha);
    }

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0) * outlineColor.a);
}