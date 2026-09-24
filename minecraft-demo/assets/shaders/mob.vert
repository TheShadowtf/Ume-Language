#version 410 core
// Mob vertex shader — supports per-part model matrix and textures
// Vertex format (stride=8): [x, y, z,  r, g, b,  u, v]
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec2 aUV;

uniform mat4 mvp;
uniform mat4 uModel;       // per-part transform (offset + animation)

out vec3 vColor;
out vec2 vUV;
out vec3 vPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = mvp * vec4(aPos, 1.0);
    vColor = aColor;
    vUV = aUV;
    vPos = worldPos.xyz;
}
