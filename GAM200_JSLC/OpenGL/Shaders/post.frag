#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTex;
uniform sampler2D uLightOverlayTex;

uniform float uExposure;
uniform bool  uUseLightOverlay;
uniform float uLightOverlayStrength;

uniform vec2 uCameraPos;
uniform vec2 uGameSize;

uniform vec2 uHallwayMin;
uniform vec2 uHallwaySize;

float OverlayChannel(float base, float blend)
{
    return (base < 0.5)
        ? (2.0 * base * blend)
        : (1.0 - 2.0 * (1.0 - base) * (1.0 - blend));
}

vec3 OverlayBlend(vec3 base, vec3 blend)
{
    return vec3(
        OverlayChannel(base.r, blend.r),
        OverlayChannel(base.g, blend.g),
        OverlayChannel(base.b, blend.b)
    );
}

void main()
{
    vec3 color = texture(uSceneTex, vUV).rgb;

    color *= uExposure;

    if (uUseLightOverlay)
    {
        vec2 worldPos = vec2(
            vUV.x * uGameSize.x + (uCameraPos.x - uGameSize.x * 0.5),
            vUV.y * uGameSize.y + (uCameraPos.y - uGameSize.y * 0.5)
        );

        vec2 overlayUV = (worldPos - uHallwayMin) / uHallwaySize;

        if (overlayUV.x >= 0.0 && overlayUV.x <= 1.0 &&
            overlayUV.y >= 0.0 && overlayUV.y <= 1.0)
        {
            vec4 lightSample = texture(uLightOverlayTex, overlayUV);

            vec3 overlayed = OverlayBlend(color, lightSample.rgb);
            float factor = lightSample.a * uLightOverlayStrength;

            color = mix(color, overlayed, factor);
        }
    }

    FragColor = vec4(color, 1.0);
}