#version 410 core
// Mob fragment shader — directional lighting, cast shadow from terrain, sky ambient, and atmospheric fog
in vec3 vColor;
in vec2 vUV;
in vec3 vPos;

uniform sampler2D uTexture;
uniform int       uUseTexture;

uniform vec3  camPos;
uniform vec3  fogColor;
uniform vec3  uLightDir;
uniform vec3  uSunColor;
uniform vec3  uSkyColor;
uniform float uLightStrength;
uniform float uMinLight;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

void main() {
    vec3 dX = dFdx(vPos);
    vec3 dY = dFdy(vPos);
    vec3 N = normalize(cross(dX, dY));

    vec3 viewDir = normalize(camPos - vPos);
    if (dot(N, viewDir) < 0.0) {
        N = -N;
    }

    // Directional light + sky ambient
    float NdotL = dot(N, uLightDir);
    float diffuse = clamp(NdotL * 0.70 + 0.30, 0.0, 1.0);
    vec3 directLight = uSunColor * (diffuse * uLightStrength);

    float topFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyAmbient = mix(uSkyColor * 0.35, uSkyColor * 0.65, topFactor);

    vec3 totalLight = directLight + skyAmbient;
    totalLight = max(totalLight, vec3(uMinLight));

    vec4 baseColor = (uUseTexture != 0) ? (texture(uTexture, vUV) * vec4(vColor, 1.0)) : vec4(vColor, 1.0);
    if (baseColor.a < 0.1) discard;

    vec3 col = baseColor.rgb * totalLight;

    // Atmospheric distance fog
    float dist = length(vPos - camPos);
    vec3 rayDir = normalize(vPos - camPos);
    float sunGleam = max(dot(rayDir, uLightDir), 0.0);
    float sunAtmosphere = pow(sunGleam, 6.0) * 0.30 * uLightStrength;
    vec3 atmosphericFog = mix(fogColor, uSunColor, sunAtmosphere);

    float fStart = uFogEnd > 0.0 ? uFogStart : 45.0;
    float fEnd   = uFogEnd > 0.0 ? uFogEnd   : 80.0;
    float fog = clamp((dist - fStart) / max(fEnd - fStart, 1.0), 0.0, 1.0);
    fog = fog * fog;
    col = mix(col, atmosphericFog, fog * 0.85);

    FragColor = vec4(col, 1.0);
}
