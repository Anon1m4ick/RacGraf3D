#version 330 core
in vec2 UV;
out vec4 FragColor;

uniform sampler2D uTex;

void main()
{
    FragColor = texture(uTex, UV);
}
