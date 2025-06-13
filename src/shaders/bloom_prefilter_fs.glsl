#version 460

out vec4 fColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform float bloomThreshold;
uniform float softKnee; 

void main() {
    
    vec3 color = texture(sceneTexture, TexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float contribution = smoothstep(bloomThreshold - softKnee, bloomThreshold + softKnee, brightness);
    
    fColor = vec4(color * contribution, 1.0);
}