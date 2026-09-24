#version 410 core
// Terrain fragment shader
// High-fidelity voxel lighting with real cast shadows from blocks, trees, caves,
// and distant mountain ridges, dynamic directional sun/moon light,
// soft ambient occlusion, animated water with specular reflections & fresnel,
// wind-kissed foliage, and atmospheric horizon fog with sun glare.

in vec2 vUV;
in float vAO;
in float vSkyLight;
in float vObstacles;
in vec3 vPos;
in vec3 vWorldPos;

uniform sampler2D uAtlas;
uniform int       uUseTexture;
uniform vec3      uFlatColor;
uniform vec3      camPos;
uniform vec3      fogColor;
uniform vec3      uLightDir;      // Direction pointing towards sun/moon (normalized)
uniform vec3      uSunColor;      // Color of direct sunlight/moonlight
uniform vec3      uSkyColor;      // Ambient sky color
uniform float     uLightStrength; // 0.35 (night) .. 1.0 (day)
uniform float     uMinLight;
uniform float     uFogStart;
uniform float     uFogEnd;
uniform float     uTime;

out vec4 FragColor;

void main() {
    vec3 baseCol;
    float alpha = 1.0;
    bool isWater = false;
    bool isFoliage = false;

    if (uUseTexture != 0) {
        vec2 uv = vUV;

        // Detect if this is a water tile: atlas column 2, row 2 (UV x in [0.39, 0.61], y in [0.65, 1.0])
        if (vUV.x >= 0.395 && vUV.x <= 0.605 && vUV.y >= 0.65) {
            isWater = true;
            vec2 waveOffset = vec2(
                sin(vWorldPos.x * 2.0 + uTime * 2.0) * 0.003 + cos(vWorldPos.z * 1.5 + uTime * 1.5) * 0.003,
                cos(vWorldPos.z * 2.0 + uTime * 1.8) * 0.003 + sin(vWorldPos.x * 1.5 + uTime * 1.4) * 0.003
            );
            uv = clamp(uv + waveOffset, vec2(0.401, 0.668), vec2(0.599, 0.998));
        }

        vec4 texSample = texture(uAtlas, uv);
        if (texSample.a < 0.1) {
            discard;
        }
        baseCol = texSample.rgb;
        alpha = texSample.a;

        // Tint grayscale textures: grass_top (col 0, row 0) and leaves (col 0, row 2)
        if (vUV.x <= 0.2001) {
            bool isGray = abs(texSample.r - texSample.g) < 0.02 && abs(texSample.g - texSample.b) < 0.02;
            if (isGray) {
                if (vUV.y < 0.35) {
                    baseCol *= vec3(0.70, 1.25, 0.42);
                } else if (vUV.y > 0.65) {
                    baseCol *= vec3(0.70, 1.45, 0.42);
                    isFoliage = true;
                }
            }
        }
    } else {
        baseCol = uFlatColor;
    }

    // ── 1. Geometric Normal Computation ──────────────────────────────────────
    vec3 dX = dFdx(vWorldPos);
    vec3 dY = dFdy(vWorldPos);
    vec3 N = normalize(cross(dX, dY));

    vec3 viewDir = normalize(camPos - vWorldPos);
    if (dot(N, viewDir) < 0.0) {
        N = -N;
    }

    // ── 2. Ambient Occlusion (AO) Curve ─────────────────────────────────────
    float ao = clamp(vAO, 0.2, 1.0);
    float aoCurve = pow(ao, 1.4);

    // ── 3. Shadow Casting (Overhead Blocks/Trees + Moving Directional Voxel Shadows) ──
    float shadow = clamp(vSkyLight, 0.0, 1.0);
    if (shadow > 0.01 && vObstacles > 0.5 && N.y > 0.5) {
        uint obsVal = uint(vObstacles + 0.5);
        float dh_px = float(obsVal & 0xFu);
        float dh_nx = float((obsVal >> 4u) & 0xFu);
        float dh_pz = float((obsVal >> 8u) & 0xFu);
        float dh_nz = float((obsVal >> 12u) & 0xFu);

        float fx = fract(vWorldPos.x);
        float fz = fract(vWorldPos.z);
        float sunY = max(uLightDir.y, 0.08);

        // Sun in +X direction: shadow falls toward -X from +X obstacle
        if (uLightDir.x > 0.02 && dh_px > 0.0) {
            float reach = dh_px * (uLightDir.x / sunY);
            float dist = 1.0 - fx;
            if (dist < reach) {
                float fade = smoothstep(reach, max(reach - 0.20, 0.0), dist);
                shadow = min(shadow, 1.0 - fade);
            }
        }
        // Sun in -X direction: shadow falls toward +X from -X obstacle
        if (uLightDir.x < -0.02 && dh_nx > 0.0) {
            float reach = dh_nx * ((-uLightDir.x) / sunY);
            float dist = fx;
            if (dist < reach) {
                float fade = smoothstep(reach, max(reach - 0.20, 0.0), dist);
                shadow = min(shadow, 1.0 - fade);
            }
        }
        // Sun in +Z direction: shadow falls toward -Z from +Z obstacle
        if (uLightDir.z > 0.02 && dh_pz > 0.0) {
            float reach = dh_pz * (uLightDir.z / sunY);
            float dist = 1.0 - fz;
            if (dist < reach) {
                float fade = smoothstep(reach, max(reach - 0.20, 0.0), dist);
                shadow = min(shadow, 1.0 - fade);
            }
        }
        // Sun in -Z direction: shadow falls toward +Z from -Z obstacle
        if (uLightDir.z < -0.02 && dh_nz > 0.0) {
            float reach = dh_nz * ((-uLightDir.z) / sunY);
            float dist = fz;
            if (dist < reach) {
                float fade = smoothstep(reach, max(reach - 0.20, 0.0), dist);
                shadow = min(shadow, 1.0 - fade);
            }
        }
    }

    // ── 4. Directional Sunlight & Sky Ambient ───────────────────────────────
    float NdotL = dot(N, uLightDir);
    float directDiffuse = clamp(NdotL * 0.70 + 0.30, 0.0, 1.0);

    float topFactor = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyAmbient = mix(uSkyColor * 0.20, uSkyColor * 0.40, topFactor);
    if (N.y < -0.2) {
        skyAmbient *= 0.60;
    }

    // Direct sun light contribution (properly balanced: 72% direct, 28% ambient so total sunlight is ~0.95, not washed out!)
    vec3 directLight = uSunColor * (directDiffuse * 0.72 * uLightStrength * shadow);
    vec3 ambientLight = skyAmbient * 0.40;

    vec3 totalLight = (directLight + ambientLight) * aoCurve;
    totalLight = clamp(totalLight, vec3(uMinLight), vec3(1.05));

    vec3 col = baseCol * totalLight;

    // ── 5. Foliage Wind Waves ───────────────────────────────────────────────
    if (isFoliage) {
        float wind = sin(vWorldPos.x * 1.5 + vWorldPos.z * 1.2 + uTime * 3.0) * 0.05
                   + cos(vWorldPos.y * 2.0 + uTime * 2.2) * 0.04;
        col *= (1.0 + wind);
    }

    // ── 6. Water Shading (Specular Sun Glint & Fresnel Reflections) ─────────
    if (isWater) {
        float wave1 = sin(vWorldPos.x * 2.2 + uTime * 2.5) * cos(vWorldPos.z * 2.2 + uTime * 2.0);
        float wave2 = cos(vWorldPos.x * 3.5 - uTime * 2.8) * sin(vWorldPos.z * 3.5 + uTime * 2.5);
        vec3 waterN = normalize(vec3(wave1 * 0.14 + wave2 * 0.09, 1.0, wave2 * 0.14 + wave1 * 0.09));

        vec3 halfVec = normalize(uLightDir + viewDir);
        float NdotH = max(dot(waterN, halfVec), 0.0);
        float spec = pow(NdotH, 28.0);

        // Sun specular glint across water surface (sparkling reflections)
        vec3 sunGlint = uSunColor * (spec * 1.35 * uLightStrength);
        col += sunGlint;

        // Sky Fresnel reflection on water surface
        float fresnel = pow(1.0 - max(dot(waterN, viewDir), 0.0), 2.5);
        vec3 skyReflect = mix(uSkyColor * 1.25, vec3(0.65, 0.82, 1.0), 0.45);
        col = mix(col, skyReflect, clamp(fresnel * 0.75, 0.0, 0.85));

        // Deep clear water tint
        col = mix(col, vec3(0.14, 0.38, 0.65), 0.30);
        alpha = mix(0.75, 0.95, fresnel);
    }

    // ── 7. Atmospheric Sun Glare & Distance Fog ─────────────────────────────
    float dist = length(vWorldPos - camPos);
    vec3 rayDir = normalize(vWorldPos - camPos);

    float sunGleam = max(dot(rayDir, uLightDir), 0.0);
    float sunAtmosphere = pow(sunGleam, 6.0) * 0.35 * uLightStrength;
    vec3 atmosphericFog = mix(fogColor, uSunColor, sunAtmosphere);

    float fStart = uFogEnd > 0.0 ? uFogStart : 56.0;
    float fEnd   = uFogEnd > 0.0 ? uFogEnd   : 101.0;
    float fog = clamp((dist - fStart) / max(fEnd - fStart, 1.0), 0.0, 1.0);
    fog = fog * fog;
    col = mix(col, atmosphericFog, fog * 0.90);
    if (isWater) {
        alpha = mix(alpha, 1.0, fog);
    }

    col = pow(col, vec3(0.96));

    FragColor = vec4(col, alpha);
}
