#version 410 core
// Terrain fragment shader
// Textured + AO + distance fog + simple directional lighting
in vec2 vUV;
in float vAO;
in vec3 vPos;
in vec3 vWorldPos;

uniform sampler2D uAtlas;
uniform int   uUseTexture;
uniform vec3  uFlatColor;
uniform vec3  camPos;
uniform vec3  fogColor;
uniform vec3  uLightDir;     // normalized
uniform float uLightStrength; // 0.4 (night) .. 1.0 (day)
uniform float uMinLight;

out vec4 FragColor;

void main() {
    vec3 baseCol;
    if (uUseTexture != 0) {
        vec4 texSample = texture(uAtlas, vUV);
        baseCol = texSample.rgb;
    } else {
        baseCol = uFlatColor;
    }

    // Face AO * global light
    float light = vAO * uLightStrength;
    light = max(light, uMinLight);
    vec3 col = baseCol * light;

    // Distance fog
    float dist = length(vPos - camPos);
    float fog = clamp((dist - 24.0) / 22.0, 0.0, 0.78);
    FragColor = vec4(mix(col, fogColor, fog), 1.0);
}
