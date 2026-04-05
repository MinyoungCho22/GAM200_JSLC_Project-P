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
uniform vec2 uFramebufferSize;

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
    vec2 uv = vUV;
    float screenAspect = uFramebufferSize.x / max(uFramebufferSize.y, 1.0);
    float gameAspect = uGameSize.x / max(uGameSize.y, 1.0);

    // Avoid flickering between letterbox branches when aspects match in float (e.g. 2560/1440 vs 1920/1080).
    const float kAspectEps = 1.0e-4;
    float aspectDiff = screenAspect - gameAspect;

    vec2 sceneUV = uv;
    if (aspectDiff > kAspectEps)
    {
        float w = gameAspect / screenAspect;
        float left = (1.0 - w) * 0.5;
        sceneUV.x = (uv.x - left) / w;
    }
    else if (aspectDiff < -kAspectEps)
    {
        float h = screenAspect / gameAspect;
        float bottom = (1.0 - h) * 0.5;
        sceneUV.y = (uv.y - bottom) / h;
    }

    // Strict 0..1 tests cause occasional full rows/columns of black (FP error at letterbox edges).
    const float kUvEps = 1.0e-3;
    if (sceneUV.x < -kUvEps || sceneUV.x > 1.0 + kUvEps ||
        sceneUV.y < -kUvEps || sceneUV.y > 1.0 + kUvEps)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color = texture(uSceneTex, clamp(sceneUV, vec2(0.0), vec2(1.0))).rgb;

    color *= uExposure;

    if (uUseLightOverlay)
    {
        vec2 worldPos = vec2(
            sceneUV.x * uGameSize.x + (uCameraPos.x - uGameSize.x * 0.5),
            sceneUV.y * uGameSize.y + (uCameraPos.y - uGameSize.y * 0.5)
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