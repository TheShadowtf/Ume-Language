#version 410 core
// Mob vertex shader — supports per-part model matrix
// Vertex format (stride=6): [x, y, z,  r, g, b]
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;

uniform mat4 mvp;
uniform mat4 uModel;       // per-part transform (offset + animation)

out vec3 vColor;
out vec3 vPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = mvp * worldPos;
    vColor = aColor;
    vPos = worldPos.xyz;
}
