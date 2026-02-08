#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uTexture;

void main()
{
    vec4 c = texture(uTexture, vUV);
    if (c.a < 0.05) discard;
    FragColor = c;
}