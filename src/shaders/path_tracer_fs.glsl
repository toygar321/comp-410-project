#version 460

#define PI_F 3.14159265358979f
#define FLT_MAX 1e+7
#define T_MIN 0.01

layout(location = 0) out vec4 g_FinalColor;  
layout(location = 1) out vec4 g_WorldNormal; 
layout(location = 2) out vec4 g_Albedo;      
layout(location = 3) out vec4 g_WorldPos;  
layout(location = 4) out vec4 g_ObjectInfo;


const int MAX_OBJECTS = 16;
const int MAX_SPHERE_TEXTURES = 8;

struct ray {
    vec3 origin;
    vec3 direction;
};

struct material {
    vec3 albedo;
    float emission;
    float metallic;     // 0.0 for pure diffuse, 1.0 for pure metal
    float roughness;    // 0.0 for smooth/mirror, up to 1.0 for very rough
    int textureID;
};

struct hit_record {
    float t;
    vec3 p;
    vec3 normal;
    material m;
    int obj_type;
};

struct object {
    material m;
    vec4 rot_quat;
    vec3 center;
    float r1;
    float r2;
    int type;
};

layout ( std140 ) uniform object_buf {
    object objects[MAX_OBJECTS];
    int num_objects_active;
};

uniform vec4 camPos;
uniform vec4 camRot_quat;
uniform float camFov;
uniform vec2 res;
uniform float time;
uniform int frame_count;
uniform sampler2D previous_acc;
uniform sampler2D skyDomeTexture;
uniform sampler2DArray sphere_texture_array;
uniform float nearPlane;
uniform float farPlane;


vec3 point_at_parameter(ray r, float t) { return r.origin + t * r.direction; }

vec4 quat_conj(vec4 q) { 
    return vec4(-q.x, -q.y, -q.z, q.w); 
}
vec4 quat_inv(vec4 q) { 
    return quat_conj(q) * (1 / dot(q, q)); 
}
vec4 quat_mult(vec4 q1, vec4 q2) { 
	vec4 qr;
	qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
	qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
	qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
	qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
	return qr;
}
vec3 rotate(vec4 qr, vec3 v) { 
	vec4 qr_conj = quat_conj(qr);
	vec4 q_pos = vec4(v.xyz, 0.0);
	vec4 q_tmp = quat_mult(qr, q_pos);
	return quat_mult(q_tmp, qr_conj).xyz;
}
vec3 getRayDir(vec2 offset) {
    vec2 coord = gl_FragCoord.xy + offset;
	vec2 ndc = (coord / res - 0.5) * 2.0;
    float aspectRatio = res.x / res.y;
    float half_h = tan(camFov / 2.0);

    vec3 initial_dir;
    initial_dir.x = ndc.x * half_h * aspectRatio;
    initial_dir.y = ndc.y * half_h;
    initial_dir.z = -1.0;
	return normalize(rotate(camRot_quat, initial_dir));
}

uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}
uint hash( uvec3 v ) { 
    return hash( v.x ^ hash(v.y) ^ hash(v.z));
}
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}
float random( vec2  v, float seed ) { 
    return floatConstruct(hash(floatBitsToUint(vec3(v, seed)))); 
}

vec3 random_vector(float seed) {
    float r1 = random(gl_FragCoord.xy, seed + 0.17);       
    float r2 = random(gl_FragCoord.xy, seed + 17.3 * r1); 
    float r3 = random(gl_FragCoord.xy, seed + 31.71 + r2);

    float theta = 2.0 * PI_F * r1;         
    float phi   = acos(2.0 * r2 - 1.0);                  
    float r_val = pow(r3, 1.0/3.0);      
    vec3 p;
    p.x = r_val * sin(phi) * cos(theta);
    p.y = r_val * sin(phi) * sin(theta);
    p.z = r_val * cos(phi);
    return normalize(p);
}

void sample_object(const object o, float seed, out vec3 point_on_surface, out vec3 normal_on_surface) {
    if (o.type == 0) {
        float theta = 2.0 * PI_F * random(gl_FragCoord.xy, seed);
        float phi = acos(1.0 - 2.0 * random(gl_FragCoord.xy, seed * 1.9387));
        vec3 random_dir = vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));

        point_on_surface = o.center + rotate(o.rot_quat, o.r1 * random_dir);
        normal_on_surface = rotate(o.rot_quat, random_dir);
    }
    else if (o.type == 1) {
        float r1 = random(gl_FragCoord.xy, seed);
        float r2 = random(gl_FragCoord.xy, seed * 1.7601);

        float theta = 2.0 * PI_F * r1;
        float radius_sq = o.r2*o.r2 + r2 * (o.r1*o.r1 - o.r2*o.r2);
        float radius = sqrt(radius_sq);

        // Point on a base ring (XY plane)
        vec3 local_point = vec3(radius * cos(theta), 0.0, radius * sin(theta));
        vec3 local_normal = vec3(0.0, 1.0, 0.0);

        point_on_surface = o.center + rotate(o.rot_quat, local_point);
        normal_on_surface = rotate(o.rot_quat, local_normal);
    }
}

vec2 get_object_uv(const object o, const hit_record rec) {
    vec4 inv_rot = quat_inv(o.rot_quat);
    if (o.type == 0) {
        vec3 local_normal = rotate(inv_rot, rec.normal);
        float phi = atan(local_normal.z, local_normal.x);
        float theta = acos(local_normal.y);
        float u = 1.0 - ((phi + PI_F) / (2.0 * PI_F));
        float v = 1.0 - (theta / PI_F);
        return vec2(u, v);
    }
    else if (o.type == 1) {
        vec3 local_p = rotate(inv_rot, rec.p - o.center);
        float hit_radius = length(local_p.xz);
        float v = (hit_radius - o.r2) / (o.r1 - o.r2);

        float angle = atan(local_p.z, local_p.x);
        float u = (angle + PI_F) / (2.0 * PI_F);
        return vec2(v, u);
    }
    return vec2(0.0);
}
bool hit_object(const object o, const ray r, float t_min, float t_max, inout hit_record rec) {
    
    vec4 inv_rot_quat = quat_inv(o.rot_quat);
    ray local_ray;
    local_ray.origin = rotate(inv_rot_quat, r.origin - o.center);
    local_ray.direction = rotate(inv_rot_quat, r.direction);
    
    bool hit_found = false;
    float t_hit = 0.0;
    vec3 local_p, local_normal;
    
    if (o.type == 0) {
        vec3 oc = local_ray.origin;
        float a = dot(local_ray.direction, local_ray.direction);
        float b = dot(oc, local_ray.direction);
        float c = dot(oc, oc) - o.r1 * o.r1;
        float discriminant = b*b - a*c;
        if (discriminant > 0) {
            float temp = (-b - sqrt(discriminant)) / a;
            if (temp < t_max && temp > t_min) {
                hit_found = true;
                t_hit = temp;
                local_p = point_at_parameter(local_ray, t_hit);
                local_normal = local_p / o.r1;
            } else {
                 temp = (-b + sqrt(discriminant)) / a;
                 if (temp < t_max && temp > t_min) {
                    hit_found = true;
                    t_hit = temp;
                    local_p = point_at_parameter(local_ray, t_hit);
                    local_normal = local_p / o.r1;
                 }
            }
        }
    }
    else if (o.type == 1) {
        // Ring normal in local space is (0,1,0)
        vec3 ring_local_normal = vec3(0.0, 1.0, 0.0);
        float denom = dot(local_ray.direction, ring_local_normal);
        if (abs(denom) > 1e-6) {
            // No need to subtract center, it's at origin in local space
            float t = -dot(local_ray.origin, ring_local_normal) / denom;
            if (t > t_min && t < t_max) {
                vec3 hit_p = point_at_parameter(local_ray, t);
                float dist_sq = dot(hit_p.xz, hit_p.xz); // Check distance on XZ plane
                if (dist_sq < o.r1 * o.r1 && dist_sq > o.r2 * o.r2) {
                    hit_found = true;
                    t_hit = t;
                    local_p = hit_p;
                    local_normal = (denom < 0) ? ring_local_normal : -ring_local_normal;
                }
            }
        }
    }

    if (hit_found) {
        rec.t = t_hit;
        // --- Transform results back to World Space ---
        rec.p = rotate(o.rot_quat, local_p) + o.center;
        rec.normal = rotate(o.rot_quat, local_normal);
        rec.m = o.m;
        rec.obj_type = o.type;
        return true;
    }
    return false;
}

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI_F * denom * denom;

    return nom / max(denom, 0.0001);
}
float G_SchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / max(denom, 0.0001);
}
float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = G_SchlickGGX(NdotV, roughness);
    float ggx1 = G_SchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
vec3 F_FresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 importance_sample_GGX(vec2 rand_uv, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI_F * rand_uv.x;
    float cosTheta = sqrt((1.0 - rand_uv.y) / (1.0 + (a*a - 1.0) * rand_uv.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void scatter(const ray r_in, const hit_record rec, const vec3 surface_albedo, inout vec3 attenuation, inout ray scattered, float seed, inout bool was_specular_scatter) {
    vec3 N = rec.normal;
    vec3 V = -r_in.direction;

    vec3 f0 = mix(vec3(0.04), surface_albedo, rec.m.metallic);
    vec3 diffuse_color = surface_albedo * (1.0 - rec.m.metallic);

    vec3 f = F_FresnelSchlick(max(dot(N, V), 0.0), f0);
    float specular_chance = max(f.r, max(f.g, f.b));
    float diffuse_chance = 1.0 - specular_chance;

    float r1 = random(gl_FragCoord.xy, seed + 0.237);

    if (r1 < diffuse_chance) {
        vec3 scatter_dir = N + random_vector(seed + 2.271);
        if (dot(scatter_dir, N) <= 0.0) scatter_dir = -scatter_dir;

        scattered = ray(rec.p, normalize(scatter_dir));
        attenuation = diffuse_color;
        was_specular_scatter = false;
    }
    else {
        float r2 = random(gl_FragCoord.xy, seed + 1.29);
        float r3 = random(gl_FragCoord.xy, seed + 3.41);
        vec3 H = importance_sample_GGX(vec2(r2, r3), N, rec.m.roughness);
        vec3 L = reflect(-V, H);

        if (dot(L, N) <= 0.0) {
            attenuation = vec3(0.0);
            was_specular_scatter = true;
            scattered = ray(rec.p, N);
        }
        else {
            scattered = ray(rec.p, normalize(L));
            attenuation = f;
            was_specular_scatter = true;
        }
    }

    float path_prob = diffuse_chance + specular_chance;
    if (path_prob > 0.0) attenuation /= path_prob;
}
vec3 emitted(material m) {
    return m.albedo * m.emission;
}

vec3 color(ray r, float seed) { // path tracing wiht next event estimation
    vec3 accumulated_light = vec3(0.0);
    vec3 current_throughput = vec3(1.0); 
    ray cr = r;
    bool previous_event_was_specular = true;

    const int MAX_BOUNCES = 5; 

    for (int i = 0; i < MAX_BOUNCES; i++) {
        hit_record rec;
        bool hit = false;
        float closest_t = FLT_MAX; 
        object hit_obj;

        for (int j = 0; j < num_objects_active; j++) {
            object o = objects[j];
            hit_record temp_rec;
            
            if (hit_object(o, cr, T_MIN, closest_t, temp_rec)) {
                hit = true;
                closest_t = temp_rec.t;
                rec = temp_rec;
                hit_obj = o;
            }
        }

        if (hit) {
            vec3 surface_albedo = rec.m.albedo;
            float alpha = 1.0;

            if (rec.m.textureID > -1) {
                vec2 uv = get_object_uv(hit_obj, rec);
                vec4 tex_color = texture(sphere_texture_array, vec3(uv, float(rec.m.textureID)));
                surface_albedo *= tex_color.rgb;
                alpha = tex_color.a;
            }

            if (alpha < 0.99) {
                if (random(gl_FragCoord.xy, seed + float(i) * 17.71) < (1.0 - alpha)) {
                    cr = ray(rec.p, cr.direction);
                    i--;
                    continue;
                }
            }

            if (rec.m.emission > 0.05 && previous_event_was_specular) {
                float max_indirect_contrib = 10000.0;
                if (i > 0) max_indirect_contrib = 10.0;
                accumulated_light += current_throughput * min(emitted(rec.m), vec3(max_indirect_contrib));
            }
            
            for (int j = 0; j < num_objects_active; j++) {
                object light = objects[j];
                if (light.m.emission < 0.05) continue;

                float light_area;
                if (light.type == 0) light_area = 4.0 * PI_F * light.r1 * light.r1;
                else if (light.type == 1) light_area = PI_F * (light.r1 * light.r1 - light.r2 * light.r2);
                else continue;

                vec3 p_on_light, n_on_light;
                sample_object(light, seed + float(j) * 5.37, p_on_light, n_on_light);

                vec3 to_light = p_on_light - rec.p;
                float dist_to_light_sq = dot(to_light, to_light);
                if (dist_to_light_sq < 0.0001) continue;
                float dist_to_light = sqrt(dist_to_light_sq);
                vec3 dir_to_light = to_light / dist_to_light;

                bool occluded = false;
                for (int k = 0; k < num_objects_active; k++) {
                    if (k == j) continue;

                    object occluder = objects[k];
                    hit_record shadow_rec;
                    if (hit_object(occluder, ray(rec.p, dir_to_light), T_MIN, dist_to_light - T_MIN, shadow_rec)) {
                        float shadow_alpha = 1.0; 
                        if (occluder.m.textureID > -1) {
                            vec2 shadow_uv = get_object_uv(occluder, shadow_rec);
                            shadow_alpha = texture(sphere_texture_array, vec3(shadow_uv, float(occluder.m.textureID))).a;
                        }
                        if (shadow_alpha < 0.99) {
                            if (random(gl_FragCoord.xy, seed + float(k) * 41.19) < (1.0 - shadow_alpha)) 
                                continue;
                        }
                        occluded = true;
                        break;
                    }
                } 

                if (!occluded) {
                    float cos_theta_light = max(0.0, abs(dot(n_on_light, -dir_to_light)));
                    if (cos_theta_light < 0.0001) continue;
                    
                    vec3 N = rec.normal;
                    vec3 L = dir_to_light;
                    vec3 V = -cr.direction;
                    vec3 H = normalize(V + L);
                    float NdotL = max(dot(N, L), 0.0);

                    if (rec.obj_type == 1) {
                        float NdotL = abs(dot(N, L));
                        if (NdotL > 0.0) {
                            vec3 brdf = surface_albedo / PI_F;
                            float incoming_light_geom = (NdotL * cos_theta_light * light_area) / dist_to_light_sq;
                            vec3 direct_light = emitted(light.m) * brdf * incoming_light_geom;

                            float grazing_angle_factor = 1.0 - NdotL;
                            float rim_power = 3.5;
                            float rim_intensity = 0.2; 
                            vec3 rim_color = pow(grazing_angle_factor, rim_power) * rim_intensity * surface_albedo;
                            
                            float rim_geom = (cos_theta_light * light_area) / dist_to_light_sq;
                            vec3 rim_contribution = emitted(light.m) * rim_color * rim_geom;
                            
                            direct_light += rim_contribution;
                            
                            accumulated_light += current_throughput * direct_light;
                        }
                    }
                    else {
                        float NdotL = max(dot(N, L), 0.0);
                        if (NdotL > 0.0) {
                            vec3 H = normalize(V + L);
                            vec3 f0 = mix(vec3(0.04), surface_albedo, rec.m.metallic);
                            vec3 F = F_FresnelSchlick(max(dot(H, V), 0.0), f0);
                            
                            float D = D_GGX(N, H, rec.m.roughness);
                            float G = G_Smith(N, V, L, rec.m.roughness);
                            vec3 specular_brdf = (D * G * F) / max(4.0 * max(dot(N, V), 0.0) * NdotL, 0.001);
                            
                            vec3 kS = F;
                            vec3 kD = (vec3(1.0) - kS) * (1.0 - rec.m.metallic);
                            vec3 diffuse_brdf = kD * surface_albedo / PI_F;

                            vec3 brdf = diffuse_brdf + specular_brdf;
                            float geometry_term = (NdotL * cos_theta_light * light_area) / dist_to_light_sq;
                            vec3 direct_light = emitted(light.m) * brdf * geometry_term;
                            
                            accumulated_light += current_throughput * direct_light;
                        }
                    }
                }
            }
 
            ray scattered;
            vec3 attenuation_from_scatter;
            bool current_scatter_was_specular;

            scatter(cr, rec, surface_albedo, attenuation_from_scatter, scattered, seed * 1.501, current_scatter_was_specular);
            current_throughput *= attenuation_from_scatter;
            cr = scattered;
            previous_event_was_specular = current_scatter_was_specular;
            
            float m = max(current_throughput.r, max(current_throughput.g, current_throughput.b));
            if (m < 0.00001) 
                break; 

            if (i > 1) {
                m = min(m, 0.95);

                if (random(gl_FragCoord.xy, seed + float(i) * 101.3) > m) 
                    break; 
                current_throughput /= m;
            }
        } 
        else {
            vec3 ray_dir = normalize(cr.direction);
            float phi = atan(ray_dir.z, ray_dir.x);
            float theta = acos(ray_dir.y);
            vec2 sky_uv;
            sky_uv.x = (phi + PI_F) / (2.0 * PI_F);
            sky_uv.y = 1 - (theta / PI_F);

            vec3 sky_color = texture(skyDomeTexture, sky_uv).rgb;

            accumulated_light += current_throughput * sky_color * 0.75; 

            break; 
        }
    }
    return accumulated_light;
}

vec3 color2(ray r, float seed) { // just path tracing
    vec3 accumulated_light = vec3(0.0);
    vec3 current_throughput = vec3(1.0); 
    ray cr = r;

    const int MAX_BOUNCES = 5; 

    for (int i = 0; i < MAX_BOUNCES; i++) {
        hit_record rec;
        bool hit = false;
        float closest_t = FLT_MAX; 
        object hit_obj;

        for (int j = 0; j < num_objects_active; j++) {
            object o = objects[j];

            hit_record temp_rec;
            if (hit_object(o, cr, T_MIN, closest_t, temp_rec)) {
                hit = true;
                closest_t = temp_rec.t;
                rec = temp_rec;
                hit_obj = o;
            }
        }

        if (hit) {
            accumulated_light += current_throughput * emitted(rec.m);

            vec3 surface_albedo = rec.m.albedo;
            float alpha = 1.0;

            if (rec.m.textureID > -1) {
                vec2 uv = get_object_uv(hit_obj, rec);
                vec4 tex_color = texture(sphere_texture_array, vec3(uv, float(rec.m.textureID)));
                surface_albedo *= tex_color.rgb;
                alpha = tex_color.a;
            }

            if (alpha < 0.99) {
                float rand = random(gl_FragCoord.xy, seed + float(i) * 17.71);
                if (rand < (1.0 - alpha)) {
                    cr = ray(rec.p, cr.direction);
                    continue;
                }
            }

            ray scattered;
            vec3 attenuation_from_scatter;
            bool was_specular_scatter; // Not used here, but scatter needs it.

            scatter(cr, rec, surface_albedo, attenuation_from_scatter, scattered, seed, was_specular_scatter);
            current_throughput *= attenuation_from_scatter;
            cr = scattered;

            if (max(current_throughput.r, max(current_throughput.g, current_throughput.b)) < 0.001) 
                break; 
        } 
        else {
            vec3 ray_dir = normalize(cr.direction);
            float phi = atan(ray_dir.z, ray_dir.x);
            float theta = acos(ray_dir.y);
            vec2 sky_uv;
            sky_uv.x = (phi + PI_F) / (2.0 * PI_F);
            sky_uv.y = 1.0 - (theta / PI_F);
            vec3 sky_color = texture(skyDomeTexture, sky_uv).rgb;

            accumulated_light += current_throughput * sky_color; 

            break; 
        }
    }
    return accumulated_light;
}

vec3 getCurentColor(int n) {
    vec3 col = vec3(0.0);
    for (int i = 1; i <= n; i++) {
        float rx = random(gl_FragCoord.xy, time * i);
        float ry = random(gl_FragCoord.xy, time + rx * i);
        vec2 offset = vec2(rx - 0.5, ry - 0.5);
        ray r = ray(camPos.xyz, getRayDir(offset));
        col += color(r, time * i) / float(n);
    }
    return col;
}

float viewZToNDC(float viewZ) {
    float A = -(farPlane + nearPlane) / (farPlane - nearPlane);
    float B = -(2.0 * farPlane * nearPlane) / (farPlane - nearPlane);
    return (A * viewZ + B) / -viewZ;
}

void main() {

    vec3 hdr_color = getCurentColor(9);

    if (isinf(hdr_color.x) || isinf(hdr_color.y) || isinf(hdr_color.z) ||
        isnan(hdr_color.x) || isnan(hdr_color.y) || isnan(hdr_color.z)) {
        hdr_color = vec3(0.0); 
    }

    if (frame_count > 1) {
        vec3 previous_color = texture(previous_acc, gl_FragCoord.xy / res).rgb;
        float blend_f = 1.0 / float(frame_count);
        hdr_color = mix(previous_color, hdr_color, blend_f);
    }

    g_FinalColor = vec4(hdr_color, 1.0);

    hit_record gbuffer_rec;
    ray r = ray(camPos.xyz, getRayDir(vec2(0.0)));
    bool hit = false;
    float closest_t = FLT_MAX;
    object hit_obj;
    int obj_id = -1;
    for (int j = 0; j < num_objects_active; j++) {
        object o = objects[j];
        if (hit_object(o, r, T_MIN, closest_t, gbuffer_rec)) {
            hit = true;
            closest_t = gbuffer_rec.t;
            hit_obj = o;
            obj_id = j;
        }
    }

    if (hit) {
        g_WorldNormal = vec4(normalize(gbuffer_rec.normal), 1.0);

        vec4 surface_albedo = vec4(gbuffer_rec.m.albedo, 1.0);
        if (gbuffer_rec.m.textureID > -1) {
            vec2 uv = get_object_uv(hit_obj, gbuffer_rec);
            vec4 tex_color = texture(sphere_texture_array, vec3(uv, float(gbuffer_rec.m.textureID)));
            surface_albedo *= tex_color;
        }
        g_Albedo = surface_albedo;

        vec3 world_p = gbuffer_rec.p;
        vec3 view_p = rotate(quat_inv(camRot_quat), world_p - camPos.xyz);
        float ndcDepth = viewZToNDC(view_p.z);
        g_WorldPos = vec4(view_p, ndcDepth); 

        g_ObjectInfo = vec4(float(obj_id), gbuffer_rec.m.metallic, gbuffer_rec.m.roughness, 1.0);
    } 
    else {
        g_WorldNormal = vec4(0.0);
        g_Albedo = vec4(0.0);
        g_WorldPos = vec4(0.0, 0.0, 1.0e18, 1.0);
        g_ObjectInfo = vec4(-1.0, 0.0, 0.0, 1.0);
    }
}