#version 460

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D colorTex; // Temporally accumulated color
uniform sampler2D g_normal;
uniform sampler2D g_position;
uniform sampler2D g_albedo;
uniform sampler2D g_objectInfo; // .g=metallic

uniform int stepWidth;

const float sigma_normal = 0.1;
const float sigma_position = 0.1;
const float sigma_albedo = 0.02;
const float sigma_reflect  = 0.000001;

const float sigma_color    = 0.04;

void main() {
    
    vec3 centerColor = texture(colorTex, TexCoords).rgb;
    vec3 centerPosition = texture(g_position, TexCoords).rgb;
    vec3 centerNormal = texture(g_normal, TexCoords).rgb;
    vec3 centerAlbedo = texture(g_albedo, TexCoords).rgb;
    float centerMetallic = texture(g_objectInfo, TexCoords).g;
    float centerAlpha =  texture(g_albedo, TexCoords).a;

    if (false || centerPosition.z > 1.0e17) {
        FragColor = vec4(centerColor, 1.0);
        return;
    }

    float totalWeight = 1.0;
    vec3 finalColor = centerColor;

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x == 0 && y == 0) continue;

            ivec2 sample_pix = ivec2(gl_FragCoord.xy) + ivec2(x, y) * stepWidth;
            vec2 sample_uv = sample_pix / vec2(textureSize(colorTex, 0));

            float finalWeight = 0.0;

            vec3 neighborColor = texture(colorTex, sample_uv).rgb;
            vec3 neighborPosition = texture(g_position, sample_uv).rgb;
            vec3 neighborNormal = texture(g_normal, sample_uv).rgb;
            vec3 neighborAlbedo = texture(g_albedo, sample_uv).rgb;
            float neighborAlpha = texture(g_albedo, sample_uv).a;

            float w_pos = exp(-distance(centerPosition, neighborPosition) / sigma_position);
            float w_norm = exp(-acos(clamp(dot(centerNormal, neighborNormal), -1.0, 1.0)) / sigma_normal);
            float w_alpha = (min(centerAlpha, neighborAlpha) + 0.1) / 1.1;
            float w_color = exp(-distance(centerColor, neighborColor) / sigma_color);

            float w_alb = exp(-distance(centerAlbedo, neighborAlbedo) / sigma_albedo);
            float w_reflect = exp(-distance(centerColor, neighborColor) / sigma_reflect);
            float content_weight = mix(min(w_color, w_alb), w_reflect, centerMetallic);

            finalWeight = w_pos * w_norm * content_weight * w_alpha;

            finalColor += finalWeight * neighborColor;            
            totalWeight += finalWeight;
        }
    }
    
    FragColor = vec4(finalColor / totalWeight, 1.0);
}