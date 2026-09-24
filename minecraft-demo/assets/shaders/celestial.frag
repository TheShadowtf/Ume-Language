#version 410 core
// Celestial fragment shader — procedural square Sun with corona and square Moon
in vec2 vUV;

uniform vec3  uColor;
uniform float uAlpha;
uniform int   uIsMoon;

out vec4 FragColor;

void main() {
    vec2 p = vUV * 2.0 - vec2(1.0); // -1.0 to 1.0
    float maxDist = max(abs(p.x), abs(p.y)); // square metric

    if (maxDist > 1.0) discard;

    if (uIsMoon == 0) {
        // Sun: Brilliant glowing square with soft outer corona edge
        float edge = smoothstep(1.0, 0.90, maxDist);
        vec3 col = mix(uColor * 1.35, uColor, maxDist * 0.7);
        FragColor = vec4(col, uAlpha * edge);
    } else {
        // Moon: Pale lunar square with subtle crater details
        float edge = smoothstep(1.0, 0.94, maxDist);
        float crater = sin(p.x * 14.0) * cos(p.y * 14.0) * 0.06
                     + sin(p.x * 28.0 + 1.2) * 0.03;
        vec3 col = (uColor + vec3(crater)) * 1.25;
        FragColor = vec4(col, uAlpha * edge);
    }
}
