#version 410 core
// HUD vertex shader — draws a unit quad into a screen rect (uRect)
layout(location=0) in vec3 aPos;     // -0.5..0.5

uniform vec4  uRect;       // x, y, w, h (screen px, top-left origin)
uniform float uScreenW;
uniform float uScreenH;
uniform float uUVScale;    // 1.0 = full UV, used for tiled backgrounds

out vec2 vUV;

void main() {
    float u = aPos.x + 0.5;
    float v = aPos.y + 0.5;
    float nx = (uRect.x + u * uRect.z) / uScreenW * 2.0 - 1.0;
    float ny = 1.0 - (uRect.y + (1.0 - v) * uRect.w) / uScreenH * 2.0;
    gl_Position = vec4(nx, ny, 0.0, 1.0);
    vUV = vec2(u * uUVScale, (1.0 - v) * uUVScale);
}
