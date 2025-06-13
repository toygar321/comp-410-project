#version 460 core

in float vAge;
in vec4 vClipPosition;

out vec4 FragColor;

uniform vec3 trailColor;
uniform sampler2D gbufferData; 

void main() {

    vec3 ndc = vClipPosition.xyz / vClipPosition.w;
    vec2 texCoord = ndc.xy * 0.5 + 0.5;
    float sceneNdcDepth = texture(gbufferData, texCoord).w;

    if (ndc.z > sceneNdcDepth + 0.00001) discard;
    
    float alpha = 1.0 - vAge;
    FragColor = vec4(trailColor, alpha * alpha);
}