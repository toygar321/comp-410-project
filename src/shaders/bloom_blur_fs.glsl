#version 460

out vec4 fColor;
in vec2 TexCoords;

uniform sampler2D sourceTexture;
uniform vec2 blur_direction; // (1.0, 0.0) for horizontal, (0.0, 1.0) for vertical

void main() {
    vec2 tex_size = textureSize(sourceTexture, 0);
    vec2 texelSize = 1.0 / tex_size;
    
    vec2 step = texelSize * blur_direction;

    if (blur_direction.y > 0.5) { 
        float aspectRatio = tex_size.x / tex_size.y;
        step.y /= aspectRatio;
    }

    vec3 result = vec3(0.0);
    
    float weights[4] = float[](0.227027, 0.1945946, 0.1216216, 0.054054);
    
    result += texture(sourceTexture, TexCoords).rgb * weights[0];
    for(int i = 1; i < 4; ++i) {
        result += texture(sourceTexture, TexCoords + float(i) * step).rgb * weights[i];
        result += texture(sourceTexture, TexCoords - float(i) * step).rgb * weights[i];
    }
    
    fColor = vec4(result, 1.0);
}