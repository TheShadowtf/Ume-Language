#version 410 core
// Terrain vertex shader
// Vertex format (stride=8): [x, y, z,  ao, skylight, 0,  u, v]
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aExtra;   // aExtra.x = AO brightness, aExtra.y = Skylight exposure
layout(location=2) in vec2 aUV;

uniform mat4 mvp;
uniform mat4 uModel;

out vec2 vUV;
out float vAO;
out float vSkyLight;
out float vObstacles;
out vec3 vPos;
out vec3 vWorldPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = mvp * worldPos;
    vUV = aUV;
    vAO = aExtra.x;
    vSkyLight = aExtra.y;
    vObstacles = aExtra.z;
    vPos = worldPos.xyz;
    vWorldPos = worldPos.xyz;
}
