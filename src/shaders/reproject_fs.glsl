#version 460

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D g_noisyColor;
uniform sampler2D g_normal;
uniform sampler2D g_position;
uniform sampler2D g_objectInfo; // .r = objectID, .g = metallic, .b = roughness

uniform sampler2D prev_denoisedColor;
uniform sampler2D prev_position;
uniform sampler2D prev_objectInfo;

uniform vec4 camPos;
uniform vec4 camRot_quat;
uniform vec4 prev_camPos;
uniform vec4 prev_camRot_quat;
uniform float camFov;
uniform vec2 res;

const float ALPHA = 0.1;

vec4 quat_conj(vec4 q) { return vec4(-q.x, -q.y, -q.z, q.w); }
vec4 quat_inv(vec4 q) { return quat_conj(q) * (1.0 / dot(q, q)); }
vec4 quat_mult(vec4 q1, vec4 q2) { 
	vec4 qr;
	qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
	qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
	qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
	qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
	return qr;
}
vec3 rotate(vec4 qr, vec3 v) { 
	vec4 p = vec4(v, 0.0);
	return quat_mult(quat_mult(qr, p), quat_conj(qr)).xyz;
}


void main() {
    vec3 noisyColor = texture(g_noisyColor, TexCoords).rgb;
    vec3 viewPos   = texture(g_position, TexCoords).rgb;
    vec4 objectInfo = texture(g_objectInfo, TexCoords);

    if (false || viewPos.z > 1.0e17) {
        FragColor = vec4(noisyColor, 1.0);
        return;
    }

    vec3 worldPos = rotate(camRot_quat, viewPos) + camPos.xyz;

    vec3 reprojectedViewPos = rotate(quat_inv(prev_camRot_quat), worldPos - prev_camPos.xyz);

    vec3 historyColor = noisyColor;

    if (reprojectedViewPos.z < 0.0) { 
        float half_h = tan(camFov / 2.0); 
        float aspectRatio = res.x / res.y;
        vec2 prev_uv = vec2(
            (reprojectedViewPos.x / -reprojectedViewPos.z) / (half_h * aspectRatio),
            (reprojectedViewPos.y / -reprojectedViewPos.z) / half_h
        );
        prev_uv = (prev_uv * 0.5) + 0.5;

        if (prev_uv.x > 0.0 && prev_uv.x < 1.0 && prev_uv.y > 0.0 && prev_uv.y < 1.0) {
            vec3 prevViewPos = texture(prev_position, prev_uv).rgb;
            vec4 prev_objectInfo = texture(prev_objectInfo, prev_uv);
            bool valid_pos = distance(reprojectedViewPos, prevViewPos) < 0.1;

            bool valid_id = abs(objectInfo.r - prev_objectInfo.r) < 0.5;

            if (valid_pos && valid_id) {
                historyColor = texture(prev_denoisedColor, prev_uv).rgb;
            }
        }
    }
    
    vec3 finalColor = mix(noisyColor, historyColor, ALPHA);

    FragColor = vec4(finalColor, 1.0);
}