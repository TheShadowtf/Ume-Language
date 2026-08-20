#version 410 core
// Mob fragment shader — vertex color + directional lighting + distance fog
in vec3 vColor;
in vec3 vPos;

uniform vec3  camPos;
uniform vec3  fogColor;
uniform vec3  uLightDir;
uniform float uLightStrength;
uniform float uMinLight;

out vec4 FragColor;

void main() {
    // simple top-down bias: brighter at top, darker at bottom
    float light = uLightStrength * (0.65 + 0.35 * clamp(vPos.y * 0.1 + 0.5, 0.0, 1.0));
    light = max(light, uMinLight);
    vec3 col = vColor * light;

    float fog = clamp((length(vPos - camPos) - 20.0) / 18.0, 0.0, 0.72);
    FragColor = vec4(mix(col, fogColor, fog), 1.0);
}
