#version 410 core
// Celestial vertex shader — draws the Sun and Moon quads in the sky
layout(location=0) in vec3 aPos;

uniform mat4 mvp;
uniform mat4 uModel;

out vec2 vUV;

void main() {
    vec3 centered = vec3(aPos.x - 0.5, aPos.y - 0.5, 0.0);
    vUV = aPos.xy;
    gl_Position = mvp * uModel * vec4(centered, 1.0);
}
