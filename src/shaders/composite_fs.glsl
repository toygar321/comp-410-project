#version 460

out vec4 fColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;

uniform float bloomIntensity;

void main() {

    float gamma = 1.3;

    vec3 hdrColor = texture(sceneTexture, TexCoords).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
    
    hdrColor += bloomColor * bloomIntensity;
    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    fColor = vec4(mapped, 1.0);
}