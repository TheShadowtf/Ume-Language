#version 410 core
// HUD fragment shader — colored quad OR textured quad (slot icons etc.)
in vec2 vUV;

uniform vec4  uColor;
uniform int   uUseTexture;
uniform sampler2D uTex;
uniform int   uNineSlice;       // 0 = off, 1 = 9-slice (for panel borders)
uniform vec4  uSliceRect;       // pixel rect of center slice in atlas

out vec4 FragColor;

void main() {
    if (uUseTexture != 0) {
        vec4 tex = texture(uTex, vUV);
        FragColor = tex * uColor;
    } else {
        FragColor = uColor;
    }
}
