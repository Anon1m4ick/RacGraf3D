#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vTex;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uLightColor;

uniform sampler2D uDiffMap1;
uniform sampler2D uSpecMap1;

void main()
{
    vec3 base = texture(uDiffMap1, vTex).rgb;

    // ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * uLightColor;

    // diffuse
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    // specular 
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specMap = texture(uSpecMap1, vTex).rgb;
    vec3 specular = specularStrength * spec * uLightColor * specMap;

    vec3 result = (ambient + diffuse) * base + specular;
    FragColor = vec4(result, 1.0);
}
