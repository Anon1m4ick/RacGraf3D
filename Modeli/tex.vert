
#version 330 core
layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUV;

out vec2 vUV;

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;

void main()
{
    vUV = inUV;
    vec4 world = uM * vec4(inPos.xy, 0.0, 1.0);
    gl_Position = uP * uV * world;
}