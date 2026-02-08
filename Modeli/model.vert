#version 330 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTex; 

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTex;

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;

void main()
{
    vFragPos = vec3(uM * vec4(inPos, 1.0));
    vNormal  = mat3(transpose(inverse(uM))) * inNormal;
    vTex     = inTex;
    gl_Position = uP * uV * vec4(vFragPos, 1.0);
}
