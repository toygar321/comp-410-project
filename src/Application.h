#pragma once

#include "Angel.h"
#include "Camera.h"
#include "UBOstructs.h"
#include "SceneObject.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem> 
#include <algorithm>

vec3 quat_to_euler(const vec4& q);
vec4 euler_to_quat(const vec3& eulerDegrees);

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

    // GLFW Callbacks - static to be callable by GLFW
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

private:
    void init();
    void update();
    void render();

    void init_uniform_buffer_object();
    void update_uniform_buffer_object();

    void init_framebuffers();

    void init_textures();
    void init_sphere_texture_array(const std::vector<std::string>& paths);
    void init_sky_dome_texture(const char* filePath);

    void init_ImGui();
    void render_ImGui();
    void shutdown_ImGui();

    void save_scene_to_file(const std::string& filename);
    void load_scene_from_file(const std::string& filename);
    void scan_for_save_files();

    vec3 calculate_orbital_velocity(float parentMass, float newObjectMass, vec3 directionToNew, float distance, float eccentricity, float inclination) const;

    void add_object(ObjectType type, float mass, float distance, float eccentricity, float inclination);
    void delete_object(int obj_index);

    void init_trails();
    void update_trails(const vec3& centerOfMass);
    void render_trails();
    void cleanup_trails();

    struct TrailRenderer {
        GLuint vao = 0;
        GLuint vbo = 0;
        size_t pointCount = 0;
    };
    std::vector<TrailRenderer> trailRenderers;
    GLuint trailShader = 0;

    bool show_menu = true;

    GLFWwindow* window;
    int screenWidth, screenHeight;
    int fbWidth, fbHeight;
    float resScale;
    int renderWidth, renderHeight;
    GLuint pathTracerShader;

    struct BloomMip {
        vec2 size;
        ivec2 intSize; 
        GLuint fbo;
        GLuint texture;
        GLuint pingpong_fbo;
        GLuint pingpong_texture;
    };

    std::vector<BloomMip> bloomMipChain;
    GLuint bloomPrefilterShader;
    GLuint bloomBlurShader;
    GLuint bloomCompositeShader;

    GLuint accFBO[2];
    GLuint accTex[10];
    int curr_acc_index; // 0 or 1
    int frame_acc_count;
    float last_fov;
    float last_distance, last_yaw, last_pitch;
    vec4 last_cam_quat;
    vec4 last_cam_pos;

    vec4 prev_cam_quat;
    vec4 prev_cam_pos;

    GLuint denoiseFBO[2];
    GLuint denoiseTex[2];
    GLuint reprojectionFBO;
    GLuint reprojectionTex;

    GLuint reprojectionShader;
    GLuint atrousShader;    
   
    GLuint vao, vbo;
    GLuint uboObjects;
    GLuint objectBufBindingPoint = 0;
    
    std::vector<std::unique_ptr<SceneObject>> sceneObjects;

    std::unique_ptr<Camera> camera;
    
    GLuint sky_dome_texture_id = 0;
    GLuint sphere_texture_array_id = 0;
   
    int fps = 60;
    float dt = 1.0f/fps;
    float timeScale = 1.0f;
    float last_timeScale = 1.0f;
    bool gravityEnabled = true;
    float gravitationalConstant = 0.5f;

    bool showAddObjectPopup = false;
    float newObjectMass = 1.0f;
    float newObjectDistance = 10.0f;
    float newObjectEccentricity = 0.0f; // 0 = perfect circle
    float newObjectInclination = 0.0f;  // degrees

    int selectedObjectIndex = 0;
    int lastSelectedObjectIndex = 0;
    float lastX, lastY;
    bool isOrbiting = false;

    std::vector<std::string> saveFiles;
    int selectedSaveFile = 0;

    // Member versions of callbacks
    void M_KeyCallback(int key, int scancode, int action, int mods);
    void M_MouseButtonCallback(int button, int action, int mods);
    void M_CursorPosCallback(double xpos, double ypos);
    void M_ScrollCallback(double xoffset, double yoffset);
    void M_FramebufferSizeCallback(int width, int height);

};