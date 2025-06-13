#include "Application.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

vec3 quat_to_euler(const vec4& q) {
    vec3 angles;

    double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    angles.x = std::atan2(sinr_cosp, cosr_cosp);

    double sinp = 2 * (q.w * q.y - q.z * q.x);
    if (std::abs(sinp) >= 1)
        angles.y = std::copysign(M_PI / 2, sinp); 
    else
        angles.y = std::asin(sinp);

    double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    angles.z = std::atan2(siny_cosp, cosy_cosp);

    return angles * (180.0f / M_PI); 
}

vec4 euler_to_quat(const vec3& eulerDegrees) {
    vec3 eulerRadians = eulerDegrees * (M_PI / 180.0f);
    
    vec4 qx = quat_from_axis_angle(vec3(1.0, 0.0, 0.0), eulerRadians.x); // Roll
    vec4 qy = quat_from_axis_angle(vec3(0.0, 1.0, 0.0), eulerRadians.y); // Pitch
    vec4 qz = quat_from_axis_angle(vec3(0.0, 0.0, 1.0), eulerRadians.z); // Yaw

    return quat_mult(qy, quat_mult(qx, qz));
}

Application::Application(int width, int height, const char* title)
    : window(nullptr),
      screenWidth(width), screenHeight(height),
      fbWidth(0), fbHeight(0),
      resScale(0.5f), 
      renderWidth(0), renderHeight(0),
      pathTracerShader(0),
      bloomPrefilterShader(0), bloomBlurShader(0), bloomCompositeShader(0),
      reprojectionShader(0), atrousShader(0),
      vao(0), vbo(0),
      uboObjects(0), objectBufBindingPoint(0),
      curr_acc_index(0), frame_acc_count(1),
      last_fov(0.0f),
      reprojectionFBO(0), reprojectionTex(0),
      sky_dome_texture_id(0), sphere_texture_array_id(0),
      fps(60), dt(1.0f / 60.0f),
      lastX(static_cast<float>(width) / 2.0f),
      lastY(static_cast<float>(height) / 2.0f),
      isOrbiting(false) {

    accFBO[0] = accFBO[1] = 0;
    for (int i = 0; i < 10; ++i) accTex[i] = 0;
    denoiseFBO[0] = denoiseFBO[1] = 0;
    denoiseTex[0] = denoiseTex[1] = 0;
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4.6);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4.6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    window = glfwCreateWindow(screenWidth, screenHeight, title, NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // disable V-sync

    #if defined(__linux__) || defined(_WIN32)
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW\n";
            glfwDestroyWindow(window);
            glfwTerminate();
            return;
        }
    #endif

    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    renderWidth = static_cast<int>(static_cast<float>(fbWidth) * resScale);
    renderHeight = static_cast<int>(static_cast<float>(fbHeight) * resScale);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, Application::KeyCallback);
    glfwSetMouseButtonCallback(window, Application::MouseButtonCallback);
    glfwSetCursorPosCallback(window, Application::CursorPosCallback);
    glfwSetScrollCallback(window, Application::ScrollCallback);
    glfwSetFramebufferSizeCallback(window, Application::FramebufferSizeCallback);

    init();

    init_ImGui();
}

Application::~Application() {

    shutdown_ImGui();
    cleanup_trails();

    if (accFBO[0] != 0 || accFBO[1] != 0) glDeleteFramebuffers(2, accFBO);
    if (accTex[0] != 0) glDeleteTextures(10, accTex);

    if (reprojectionFBO != 0) glDeleteFramebuffers(1, &reprojectionFBO);
    if (reprojectionTex != 0) glDeleteTextures(1, &reprojectionTex);
    
    if (denoiseFBO[0] != 0 || denoiseFBO[1] != 0) glDeleteFramebuffers(2, denoiseFBO);
    if (denoiseTex[0] != 0 || denoiseTex[1] != 0) glDeleteTextures(2, denoiseTex);

    
    for (const auto& mip : bloomMipChain) {
        glDeleteFramebuffers(1, &mip.fbo);
        glDeleteTextures(1, &mip.texture);
        glDeleteFramebuffers(1, &mip.pingpong_fbo);
        glDeleteTextures(1, &mip.pingpong_texture);
    }
    bloomMipChain.clear();

    if (pathTracerShader != 0) glDeleteProgram(pathTracerShader);
    if (reprojectionShader != 0) glDeleteProgram(reprojectionShader);
    if (atrousShader != 0) glDeleteProgram(atrousShader);
    if (bloomPrefilterShader != 0) glDeleteProgram(bloomPrefilterShader);
    if (bloomBlurShader != 0) glDeleteProgram(bloomBlurShader);
    if (bloomCompositeShader != 0) glDeleteProgram(bloomCompositeShader);
    if (trailShader != 0) glDeleteProgram(trailShader);
    
    if (sky_dome_texture_id != 0) glDeleteTextures(1, &sky_dome_texture_id);
    if (sphere_texture_array_id != 0) glDeleteTextures(1, &sphere_texture_array_id);

    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (vbo != 0) glDeleteBuffers(1, &vbo);
    if (uboObjects != 0) glDeleteBuffers(1, &uboObjects);
    
    if (window) glfwDestroyWindow(window);
    glfwTerminate();  

    std::cout << "Application cleaned up and terminated." << std::endl;
}

void Application::init() {

    pathTracerShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/path_tracer_fs.glsl");

    reprojectionShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/reproject_fs.glsl");
    atrousShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/atrous_fs.glsl");

    bloomPrefilterShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/bloom_prefilter_fs.glsl");
    bloomBlurShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/bloom_blur_fs.glsl");
    bloomCompositeShader = InitShader("./src/shaders/vshader.glsl", "./src/shaders/composite_fs.glsl");
    
    trailShader = InitShader("./src/shaders/trail_vs.glsl", "./src/shaders/trail_fs.glsl");

    glUseProgram(pathTracerShader);

    vec2 vertices[] = {
		vec2(-1.0f, -1.0f), 
		vec2(1.0f, -1.0f), 
		vec2(1.0f,  1.0f), 

		vec2(-1.0f, -1.0f),
		vec2(1.0f,  1.0f),  
		vec2(-1.0f,  1.0f), 
	};

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    GLuint vPosLoc = glGetAttribLocation(pathTracerShader, "vPos");
    glEnableVertexAttribArray(vPosLoc);
    glVertexAttribPointer(vPosLoc, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

    camera = std::make_unique<Camera>();

    init_trails();
    init_textures();
    init_framebuffers();
    load_scene_from_file("./saves/empty.scene");
    init_uniform_buffer_object();
    scan_for_save_files();

    glViewport(0, 0, fbWidth, fbHeight);
}

void Application::Run() {
    if (!window) {
        std::cerr << "Application not properly initialized\n";
        return;
    }

    double currentTime = 0.0;
    double previousTime = glfwGetTime();
    double lastFPStime = glfwGetTime();
    int frameCount = 0;

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        currentTime = glfwGetTime();
        if (currentTime - previousTime >= 1.0 / static_cast<double>(fps)) {
            dt = static_cast<float>(currentTime - previousTime);
            update();
            render();
            glfwSwapBuffers(window);
            previousTime = currentTime;
            frameCount++;
        }

        if (currentTime - lastFPStime >= 1.0) {
            double f = static_cast<double>(frameCount) / (currentTime - lastFPStime);
            std::cout << "FPS: " << f << "| Distance: " << camera->Distance << " | acc_frames: " << frame_acc_count << "\n";
            frameCount = 0;
            lastFPStime = currentTime;
        }
    }
}

void Application::render() {
    // --- Camera State Update ---

    camera->UpdateCameraVectors();
    update_uniform_buffer_object();

    vec4 curr_cam_pos = camera->Position;
    vec4 curr_cam_quat = camera->OrientationQuat;
    float curr_fov = camera->Fov;   
    float curr_distance = camera->Distance;
    float curr_yaw = camera->Yaw;
    float curr_pitch = camera->Pitch;

    bool camera_is_stationary = (curr_fov == last_fov && curr_distance == last_distance && curr_yaw == last_yaw && curr_pitch == last_pitch && timeScale == last_timeScale && selectedObjectIndex == lastSelectedObjectIndex);

    if (camera_is_stationary) {

        int max_frame_count = 1;

        if (timeScale <= 0.0f) max_frame_count = 100000;
        else if (timeScale < 1.0f) max_frame_count = static_cast<int>(0.85f / timeScale + 0.15f);

        if (frame_acc_count < max_frame_count) frame_acc_count++;
    }
    else frame_acc_count = 1;

    prev_cam_pos = last_cam_pos;
    prev_cam_quat = last_cam_quat;
    last_cam_pos = curr_cam_pos;
    last_cam_quat = curr_cam_quat;
    last_fov = curr_fov;
    last_distance = curr_distance;
    last_yaw = curr_yaw;
    last_pitch = curr_pitch;
    last_timeScale = timeScale;
    lastSelectedObjectIndex = selectedObjectIndex;

    int writeIndex = curr_acc_index;
    int readIndex = 1 - curr_acc_index;

    // --- PASS 1: Ray Trace the Scene ---

    glUseProgram(pathTracerShader);
    
    glUniform4fv(glGetUniformLocation(pathTracerShader, "camPos"), 1, camera->Position);
    glUniform4fv(glGetUniformLocation(pathTracerShader, "camRot_quat"), 1, camera->OrientationQuat);
    glUniform1f(glGetUniformLocation(pathTracerShader, "camFov"), DegreesToRadians * camera->Fov);
    glUniform2f(glGetUniformLocation(pathTracerShader, "res"), (float)renderWidth, (float)renderHeight);
    glUniform1f(glGetUniformLocation(pathTracerShader, "time"), static_cast<float>(glfwGetTime()));
    glUniform1f(glGetUniformLocation(pathTracerShader, "nearPlane"), 0.01f);
    glUniform1f(glGetUniformLocation(pathTracerShader, "farPlane"), 1.0e10f);

    glBindFramebuffer(GL_FRAMEBUFFER, accFBO[writeIndex]);
    glViewport(0, 0, renderWidth, renderHeight);

    glUniform1i(glGetUniformLocation(pathTracerShader, "frame_count"), frame_acc_count);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accTex[readIndex * 5 + 0]);
    glUniform1i(glGetUniformLocation(pathTracerShader, "previous_acc"), 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, sky_dome_texture_id);
    glUniform1i(glGetUniformLocation(pathTracerShader, "skyDomeTexture"), 1);

    if (sphere_texture_array_id != 0) {
        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, sphere_texture_array_id);
        glUniform1i(glGetUniformLocation(pathTracerShader, "sphere_texture_array"), 2);
    }

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- PASS 2: Temporal Reprojection ---

    glUseProgram(reprojectionShader);
    glBindFramebuffer(GL_FRAMEBUFFER, reprojectionFBO);
    glViewport(0, 0, renderWidth, renderHeight);

    glUniform4fv(glGetUniformLocation(reprojectionShader, "camPos"), 1, camera->Position);
    glUniform4fv(glGetUniformLocation(reprojectionShader, "camRot_quat"), 1, camera->OrientationQuat);
    glUniform4fv(glGetUniformLocation(reprojectionShader, "prev_camPos"), 1, prev_cam_pos);
    glUniform4fv(glGetUniformLocation(reprojectionShader, "prev_camRot_quat"), 1, prev_cam_quat);
    glUniform1f(glGetUniformLocation(reprojectionShader, "camFov"), DegreesToRadians * camera->Fov);
    glUniform2f(glGetUniformLocation(reprojectionShader, "res"), (float)renderWidth, (float)renderHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 0]); 
    glUniform1i(glGetUniformLocation(reprojectionShader, "g_noisyColor"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 1]);
    glUniform1i(glGetUniformLocation(reprojectionShader, "g_normal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 3]); 
    glUniform1i(glGetUniformLocation(reprojectionShader, "g_position"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 4]); 
    glUniform1i(glGetUniformLocation(reprojectionShader, "g_objectInfo"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, denoiseTex[readIndex]);
    glUniform1i(glGetUniformLocation(reprojectionShader, "prev_denoisedColor"), 4);
    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, accTex[readIndex * 5 + 3]); 
    glUniform1i(glGetUniformLocation(reprojectionShader, "prev_position"), 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, accTex[readIndex * 5 + 4]); 
    glUniform1i(glGetUniformLocation(reprojectionShader, "prev_objectInfo"), 6);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- PASS 3: A-Trous Spatial Filtering ---

    glUseProgram(atrousShader);
    glViewport(0, 0, renderWidth, renderHeight);

    const int num_atrous_iterations = 4; 
    for (int i = 0; i < num_atrous_iterations; ++i) {
        int pingPongWrite = (i % 2);
        int pingPongRead = 1 - pingPongWrite;
        
        glBindFramebuffer(GL_FRAMEBUFFER, denoiseFBO[pingPongWrite]);

        glUniform1i(glGetUniformLocation(atrousShader, "stepWidth"), 1 << i);

 
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, i == 0 ? reprojectionTex : denoiseTex[pingPongRead]);
        glUniform1i(glGetUniformLocation(atrousShader, "colorTex"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 1]); 
        glUniform1i(glGetUniformLocation(atrousShader, "g_normal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 3]);
        glUniform1i(glGetUniformLocation(atrousShader, "g_position"), 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 2]); 
        glUniform1i(glGetUniformLocation(atrousShader, "g_albedo"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, accTex[writeIndex * 5 + 4]); 
        glUniform1i(glGetUniformLocation(atrousShader, "g_objectInfo"), 4);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // --- PASS 3: Prefilter ---
    
    glUseProgram(bloomPrefilterShader);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomMipChain[0].fbo);
    glViewport(0, 0, bloomMipChain[0].size.x, bloomMipChain[0].size.y);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, denoiseTex[(num_atrous_iterations - 1) % 2]);
    glUniform1i(glGetUniformLocation(bloomPrefilterShader, "sceneTexture"), 0);
    
    glUniform1f(glGetUniformLocation(bloomPrefilterShader, "bloomThreshold"), 1.0f);
    glUniform1f(glGetUniformLocation(bloomPrefilterShader, "softKnee"), 0.2f);
    
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- PASS 4: DOWNSAMPLE with two-pass blur ---

    glUseProgram(bloomBlurShader);
    glUniform1i(glGetUniformLocation(bloomBlurShader, "sourceTexture"), 0);
    glBindVertexArray(vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bloomMipChain[0].texture);

    for (size_t i = 1; i < bloomMipChain.size(); ++i) {
        const auto& mip = bloomMipChain[i];

        // HORIZONTAL PASS
        glBindFramebuffer(GL_FRAMEBUFFER, mip.pingpong_fbo);
        glViewport(0, 0, mip.intSize.x, mip.intSize.y);
        glUniform2f(glGetUniformLocation(bloomBlurShader, "blur_direction"), 1.0f, 0.0f); // Horizontal
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // VERTICAL PASS (and downsample)
        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
        glViewport(0, 0, mip.intSize.x, mip.intSize.y);
        glBindTexture(GL_TEXTURE_2D, mip.pingpong_texture);
        glUniform2f(glGetUniformLocation(bloomBlurShader, "blur_direction"), 0.0f, 1.0f); // Vertical
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindTexture(GL_TEXTURE_2D, mip.texture);
    }

    // --- PASS 5: UPSAMPLE with additive blending ---

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); 
    glBlendEquation(GL_FUNC_ADD);

    for (size_t i = bloomMipChain.size() - 1; i > 0; --i) {
        const auto& mip = bloomMipChain[i];
        const auto& prevMip = bloomMipChain[i - 1];

        // HORIZONTAL PASS
        glBindFramebuffer(GL_FRAMEBUFFER, mip.pingpong_fbo);
        glViewport(0, 0, mip.intSize.x, mip.intSize.y); 
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mip.texture); 
        glUniform2f(glGetUniformLocation(bloomBlurShader, "blur_direction"), 1.0f, 0.0f); // Horizontal
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // VERTICAL PASS: 
        glBindFramebuffer(GL_FRAMEBUFFER, prevMip.fbo); 
        glViewport(0, 0, prevMip.intSize.x, prevMip.intSize.y); 
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mip.pingpong_texture); 
        glUniform2f(glGetUniformLocation(bloomBlurShader, "blur_direction"), 0.0f, 1.0f); // Vertical
        glDrawArrays(GL_TRIANGLES, 0, 6); 
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);

    // --- PASS 6: Final Composite ---

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbWidth, fbHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(bloomCompositeShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, denoiseTex[(num_atrous_iterations - 1) % 2]);
    glUniform1i(glGetUniformLocation(bloomCompositeShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomMipChain[0].texture);
    glUniform1i(glGetUniformLocation(bloomCompositeShader, "bloomTexture"), 1);

    float bloomIntensity = 0.004f; 
    glUniform1f(glGetUniformLocation(bloomCompositeShader, "bloomIntensity"), bloomIntensity);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // --- PASS 7: Trails ---

    render_trails();

    // --- PASS 8: ImGui ---

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Host", nullptr, window_flags);
    ImGui::PopStyleVar();

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    if (show_menu) {
        render_ImGui();
    }
    
    ImGui::End(); 

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    curr_acc_index = 1 - curr_acc_index;
}

void Application::update() {
    vec3 centerOfMass(0.0f);
    if (!sceneObjects.empty()) {
        vec3 weightedPositionSum(0.0f);
        float totalMass = 0.0f;
        for (const auto& obj_ptr : sceneObjects) {
            weightedPositionSum += obj_ptr->GetPosition() * obj_ptr->Mass;
            totalMass += obj_ptr->Mass;
        }
        if (totalMass > 0.0f) {
            centerOfMass = weightedPositionSum / totalMass;
        }
    }
    
    float scaled_dt = dt * timeScale;

    if (scaled_dt > 0.0f) {

        std::vector<int> objects_to_delete;
        objects_to_delete.reserve(sceneObjects.size());

        for (int i = 0; i < sceneObjects.size(); ++i) {

            if (std::find(objects_to_delete.begin(), objects_to_delete.end(), i) != objects_to_delete.end()) {
                continue;
            }

            vec3 totalForce(0.0f);
            
            for (int j = 0; j < sceneObjects.size(); ++j) {
                if (i == j) continue;
                
                if (std::find(objects_to_delete.begin(), objects_to_delete.end(), j) != objects_to_delete.end()) {
                    continue;
                }

                SceneObject& obj_i = *sceneObjects[i];
                SceneObject& obj_j = *sceneObjects[j];
                vec3 direction = obj_j.GetPosition() - obj_i.GetPosition();
                float distanceSq = dot(direction, direction);
                float distance = sqrt(distanceSq);

                float radius_i = obj_i.GetGpuObject(0).r1;
                float radius_j = obj_j.GetGpuObject(0).r1;
                if (distance <= (radius_i + radius_j)) {


                    SceneObject* larger_obj = (obj_i.Mass > obj_j.Mass) ? &obj_i : &obj_j;
                    SceneObject* smaller_obj = (obj_i.Mass > obj_j.Mass) ? &obj_j : &obj_i;
                    int smaller_obj_index = (obj_i.Mass > obj_j.Mass) ? j : i;

                    vec3 new_velocity = (larger_obj->velocity * larger_obj->Mass + smaller_obj->velocity * smaller_obj->Mass) / (larger_obj->Mass + smaller_obj->Mass);

                    float r1_cubed = std::pow(larger_obj->GetGpuObject(0).r1, 3);
                    float r2_cubed = std::pow(smaller_obj->GetGpuObject(0).r1, 3);
                    float new_radius = std::cbrt(r1_cubed + r2_cubed);

                    larger_obj->Mass += smaller_obj->Mass;
                    larger_obj->velocity = new_velocity;
                    larger_obj->GetGpuObject(0).r1 = new_radius;
                    
                    objects_to_delete.push_back(smaller_obj_index);
                    
                    break; 
                }
                
                if (gravityEnabled) {
                    if (distanceSq < 1.0f) distanceSq = 1.0f;
                    float forceMagnitude = gravitationalConstant * (obj_i.Mass * obj_j.Mass) / distanceSq;
                    totalForce += normalize(direction) * forceMagnitude;
                }
            }
            sceneObjects[i]->ApplyForce(totalForce, scaled_dt);
        }

        if (!objects_to_delete.empty()) {
            std::sort(objects_to_delete.rbegin(), objects_to_delete.rend());
            
            for (int index : objects_to_delete) {
                sceneObjects.erase(sceneObjects.begin() + index);
            }
            init_trails();
        }

        for (auto& obj : sceneObjects) {
            obj->Update(scaled_dt);
        }

        update_trails(centerOfMass);
    }

    if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneObjects.size()) 
        camera->Target = sceneObjects[selectedObjectIndex]->GetPosition();
    else camera->Target = centerOfMass;
}

void Application::init_uniform_buffer_object() {
    size_t maxUboSize = sizeof(GPUobject) * MAX_OBJECTS_CPP + sizeof(int);

    glGenBuffers(1, &uboObjects);
    glBindBuffer(GL_UNIFORM_BUFFER, uboObjects);
    glBufferData(GL_UNIFORM_BUFFER, maxUboSize, NULL, GL_DYNAMIC_DRAW);
    GLuint blockIndex = glGetUniformBlockIndex(pathTracerShader, "object_buf");
    if (blockIndex == GL_INVALID_INDEX) 
        std::cerr << "Error: Uniform block 'object_buf' not found in shader." << std::endl;
    glUniformBlockBinding(pathTracerShader, blockIndex, objectBufBindingPoint);
    glBindBufferBase(GL_UNIFORM_BUFFER, objectBufBindingPoint, uboObjects);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Application::update_uniform_buffer_object() {
    ObjectUBOData uboData;
    int current_gpu_object_index = 0;

    for (const auto& sceneObj : sceneObjects) {
        for (size_t i = 0; i < sceneObj->GetGpuObjectCount(); ++i) {
            if (current_gpu_object_index >= MAX_OBJECTS_CPP) {
                std::cerr << "Warning: Exceeded maximum number of GPU objects!" << std::endl;
                break;
            }
            uboData.objects[current_gpu_object_index] = sceneObj->GetGpuObject(i);
            current_gpu_object_index++;
        }
    }
    uboData.num_objects_active = current_gpu_object_index;

    size_t maxUboSize = sizeof(GPUobject) * MAX_OBJECTS_CPP + sizeof(int);
    glBindBuffer(GL_UNIFORM_BUFFER, uboObjects);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, maxUboSize, &uboData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Application::init_framebuffers() {

    glGenFramebuffers(2, accFBO);
    glGenTextures(10, accTex);
    GLenum attachments[5] = { 
        GL_COLOR_ATTACHMENT0, // Final Color 
        GL_COLOR_ATTACHMENT1, // World Normal 
        GL_COLOR_ATTACHMENT2, // Albedo 
        GL_COLOR_ATTACHMENT3, // World Position 
        GL_COLOR_ATTACHMENT4  // ObjectInfo (ID, Metallic, Roughness)
    };

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, accFBO[i]);
        for (int j = 0; j < 5; j++) {
            glBindTexture(GL_TEXTURE_2D, accTex[i * 5 + j]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + j, GL_TEXTURE_2D, accTex[i * 5 + j], 0);
        }
        glDrawBuffers(5, attachments);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR::FRAMEBUFFER:: G-Buffer FBO " << i << " is not complete!" << std::endl;
    }

    glGenFramebuffers(1, &reprojectionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, reprojectionFBO);
    glGenTextures(1, &reprojectionTex);
    glBindTexture(GL_TEXTURE_2D, reprojectionTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reprojectionTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Reprojection FBO is not complete!" << std::endl;

    glGenFramebuffers(2, denoiseFBO);
    glGenTextures(2, denoiseTex);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, denoiseFBO[i]);
        glBindTexture(GL_TEXTURE_2D, denoiseTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, denoiseTex[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR::FRAMEBUFFER:: Denoise FBO " << i << " is not complete!" << std::endl;
    }

    bloomMipChain.clear(); 
    vec2 mipSize(renderWidth, renderHeight);
    ivec2 mipIntSize(renderWidth, renderHeight);

    for (int i = 0; i < 6; i++) {
        BloomMip mip;
        
        mipSize *= 0.5f;
        mipIntSize /= 2;
        mip.size = mipSize;
        mip.intSize = mipIntSize; 

        glGenFramebuffers(1, &mip.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mip.intSize.x, mip.intSize.y, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);
        
        glGenFramebuffers(1, &mip.pingpong_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mip.pingpong_fbo);
        glGenTextures(1, &mip.pingpong_texture);
        glBindTexture(GL_TEXTURE_2D, mip.pingpong_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mip.intSize.x, mip.intSize.y, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.pingpong_texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR::FRAMEBUFFER:: Bloom FBO Mip " << i << " is not complete!" << std::endl;

        bloomMipChain.push_back(mip);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    curr_acc_index = 0;
    frame_acc_count = 1;
    if (camera) {
        last_cam_pos = camera->Position;
        last_cam_quat = camera->OrientationQuat;
        last_fov = camera->Fov;
        last_distance = camera->Distance;
        last_yaw = camera->Yaw;
        last_pitch = camera->Pitch;
    }
}

void Application::init_textures() {

     init_sky_dome_texture("./textures/skydome2.jpg");

    std::vector<std::string> texture_paths = {
        "./textures/saturn_rings2.png",
        "./textures/saturn.jpg",
        "./textures/jupiter.jpg",
        "./textures/gas_giant_1.jpg",
        "./textures/gas_giant_2.png",
        "./textures/gas_giant_3.jpg",
        "./textures/moon.jpg",
        "./textures/mars.jpg",
        "./textures/makemake.jpg",
        "./textures/haumea.jpg",
        "./textures/eris.jpg",
        "./textures/earth.jpg",
        "./textures/ceres.jpg"
    };

    init_sphere_texture_array(texture_paths);
}

void Application::init_sphere_texture_array(const std::vector<std::string>& paths) {
    int Width = 4096;
    int Height = 2048;

    if (paths.empty()) return;

    glGenTextures(1, &sphere_texture_array_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, sphere_texture_array_id);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, Width, Height, paths.size());
    
    stbi_set_flip_vertically_on_load(true);

    for (int i = 0; i < paths.size(); ++i) {
        int width, height, channels;
        unsigned char* data = stbi_load(paths[i].c_str(), &width, &height, &channels, 4);

        if (data) {
            if (width != Width || height != Height) {
                 std::cerr << "Texture Array Error: Image " << paths[i] 
                           << " has dimensions " << width << "x" << height 
                           << " but array requires " << Width << "x" << Height << std::endl;
                stbi_image_free(data);
                continue; 
            }
            
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            
            std::cout << "Loaded texture '" << paths[i] << "' into texture array layer " << i << std::endl;
            stbi_image_free(data);
        } else {
            std::cerr << "Texture Array Error: Failed to load image at path: " << paths[i] << std::endl;
            std::cerr << "STB Reason: " << stbi_failure_reason() << std::endl;
        }
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void Application::init_sky_dome_texture(const char* filePath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(filePath, &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Texture Load Error: Failed to load image at path: " << filePath << std::endl;
        std::cerr << "STB Reason: " << stbi_failure_reason() << std::endl;
        return;
    }

    GLenum dataFormat = GL_RGB;
    if (channels == 4) {
        dataFormat = GL_RGBA;
    }

    glGenTextures(1, &sky_dome_texture_id);
    glBindTexture(GL_TEXTURE_2D, sky_dome_texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    std::cout << "Sky dome texture loaded successfully: " << filePath << std::endl;

}

void Application::init_ImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    float font_size = 16.0f;
    io.Fonts->AddFontFromFileTTF("./fonts/Roboto-Medium.ttf", font_size);

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460"); 

    std::cout << "ImGui initialized successfully." << std::endl;
}

void Application::shutdown_ImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::render_ImGui() {
    ImGui::Begin("Scene Controls", &show_menu);

   if (ImGui::CollapsingHeader("File Operations")) {
        // --- SAVE Section ---
        static char save_filename_buffer[128] = "new.scene";
        ImGui::InputText("Save Filename", save_filename_buffer, sizeof(save_filename_buffer));
        if (ImGui::Button("Save Scene")) {
            std::string final_path = "./saves/" + std::string(save_filename_buffer);
            if (std::filesystem::path(final_path).extension() != ".scene") {
                final_path += ".scene";
            }
            save_scene_to_file(final_path);
            scan_for_save_files();
        }

        ImGui::Separator();

        // --- LOAD Section ---
        if (saveFiles.empty()) {
            ImGui::Text("No save files found in ./saves/");
            if (ImGui::Button("Refresh List")) {
                scan_for_save_files();
            }
        } else {
            std::vector<const char*> c_style_filenames;
            for (const auto& name : saveFiles) {
                c_style_filenames.push_back(name.c_str());
            }

            ImGui::Combo("Load File", &selectedSaveFile, c_style_filenames.data(), c_style_filenames.size());
            
            if (ImGui::Button("Refresh List")) {
                scan_for_save_files();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                std::string final_path = "./saves/" + saveFiles[selectedSaveFile];
                load_scene_from_file(final_path);
            }
        }
    }

    if (ImGui::CollapsingHeader("Global Physics Settings")) {
        ImGui::Checkbox("Enable Gravity", &gravityEnabled);
        ImGui::DragFloat("Gravitational Constant", &gravitationalConstant, 0.01f, 0.0f, 10.0f);
        
        // --- IME CONTROL ---
        ImGui::Separator();
        ImGui::Text("Time Control:");
        
        if (ImGui::Button(timeScale > 0.0f ? "Pause" : "Resume")) {
            timeScale = (timeScale > 0.0f) ? 0.0f : 1.0f;
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Time Scale", &timeScale, 0.0f, 20.0f);
    }
    ImGui::Separator();

    if (ImGui::Button("Add New Scene Object...")) {
        SceneObject* parentObject = nullptr;
        if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneObjects.size()) {
            parentObject = sceneObjects[selectedObjectIndex].get();
        }
        if (parentObject) {
            float parentRadius = parentObject->GetGpuObject(0).r1;
            newObjectDistance = parentRadius * 3.0f;
            if (newObjectDistance < parentRadius + 0.5f) newObjectDistance = parentRadius + 0.5f;
        } 
        else 
            newObjectDistance = 0.0f; 
        

        newObjectEccentricity = 0.0f;
        newObjectInclination = 0.0f;
        
        showAddObjectPopup = true;
    }

    if (showAddObjectPopup) {
        ImGui::OpenPopup("Create New Object");
    }

    if (ImGui::BeginPopupModal("Create New Object", &showAddObjectPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Configure the new object to be placed in orbit around the selected target.");
        if (selectedObjectIndex == -1) {
            ImGui::TextColored(ImVec4(1,1,0,1), "Target: World Origin (0,0,0)");
        } else if (selectedObjectIndex < sceneObjects.size()){
            ImGui::TextColored(ImVec4(0,1,1,1), "Target: %s", sceneObjects[selectedObjectIndex]->Name.c_str());
        }
        ImGui::Separator();

        const char* object_types[] = { "Star", "Brown Dwarf", "Gas Giant", "Rocky Planet" };
        static int selected_type_index = 3;

        static float newObjectEditableMass = 1.0f;

        if (ImGui::Combo("Object Type", &selected_type_index, object_types, IM_ARRAYSIZE(object_types))) {
            switch(static_cast<ObjectType>(selected_type_index)) {
                case ObjectType::Star:        newObjectEditableMass = 800.0f; break;
                case ObjectType::BrownDwarf:  newObjectEditableMass = 250.0f; break;
                case ObjectType::GasGiant:    newObjectEditableMass = 80.0f;  break;
                case ObjectType::RockyPlanet: newObjectEditableMass = 1.0f;   break;
                default:                      newObjectEditableMass = 1.0f;   break;
            }
        }
       
        if (ImGui::IsWindowAppearing()) {
             switch(static_cast<ObjectType>(selected_type_index)) {
                case ObjectType::Star:        newObjectEditableMass = 800.0f; break;
                case ObjectType::BrownDwarf:  newObjectEditableMass = 250.0f; break;
                case ObjectType::GasGiant:    newObjectEditableMass = 80.0f;  break;
                case ObjectType::RockyPlanet: newObjectEditableMass = 1.0f;   break;
                default:                      newObjectEditableMass = 1.0f;   break;
            }
        }
        ImGui::DragFloat("Mass", &newObjectEditableMass, 0.1f, 0.01f, 10000.0f);

        ImGui::DragFloat("Distance from Target", &newObjectDistance, 0.2f, 1.0f, 1000.0f);
        ImGui::SliderFloat("Eccentricity", &newObjectEccentricity, 0.0f, 0.99f, "%.2f (0 = circle)");
        ImGui::SliderFloat("Inclination (degrees)", &newObjectInclination, -90.0f, 90.0f, "%.1f");
        
        // --- Creation Buttons ---
        ImGui::Separator();
        if (ImGui::Button("Create Object", ImVec2(120, 0))) {
            ObjectType typeToCreate = static_cast<ObjectType>(selected_type_index);
            add_object(typeToCreate, newObjectEditableMass, newObjectDistance, newObjectEccentricity, newObjectInclination);
            showAddObjectPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showAddObjectPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // --- Loop over SceneObjects ---
    for (int i = 0; i < sceneObjects.size(); ++i) {
        ImGui::PushID(i); 

        SceneObject& sceneObj = *sceneObjects[i];
        std::string object_label = sceneObj.Name + " " + std::to_string(i);

        if (ImGui::CollapsingHeader(object_label.c_str())) {

            const char* typeName = "Unknown";
            switch(sceneObj.Type) {
                case ObjectType::Star: typeName = "Star"; break;
                case ObjectType::BrownDwarf: typeName = "Brown Dwarf"; break;
                case ObjectType::GasGiant: typeName = "Gas Giant"; break;
                case ObjectType::RockyPlanet: typeName = "Rocky Planet"; break;
                case ObjectType::BlackHole: typeName = "Black Hole"; break;
            }
            ImGui::Text("Current Type: %s", typeName);

            if (sceneObj.Type == ObjectType::GasGiant || sceneObj.Type == ObjectType::RockyPlanet) {
                if (ImGui::Checkbox("Has Rings", &sceneObj.hasRings)) {
                    sceneObj.SetupAs(sceneObj.Type);
                }
            }

            if (ImGui::Button("Delete This Object")) {
                ImGui::OpenPopup("Confirm Deletion");
            }
            if (ImGui::BeginPopupModal("Confirm Deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to delete %s %d?", sceneObj.Name.c_str(), i);
                if (ImGui::Button("Yes, Delete")) {
                    delete_object(i);
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break; 
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::Separator();

            ImGui::Text("Transform & Physics:");
            vec3 pos = sceneObj.GetPosition();
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                sceneObj.SetPosition(pos);
                frame_acc_count = 1;
            }

            float tempMass = sceneObj.Mass;
            ImGui::InputFloat("Mass", &tempMass, 0.1f, 1.0f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                sceneObj.Mass = tempMass;
                frame_acc_count = 1;
            }
            

            vec3 vel = sceneObj.velocity;
            if (ImGui::DragFloat3("Velocity", &vel.x, 0.01f)) {
                sceneObj.velocity = vel;
            }

            vec3 eulerAngles = quat_to_euler(sceneObj.Orientation);
            
            if (ImGui::DragFloat3("Orientation (Roll, Pitch, Yaw)", &eulerAngles.x, 0.5f, -180.0f, 180.0f)) {
                sceneObj.Orientation = euler_to_quat(eulerAngles);
                frame_acc_count = 1; 
            }

            ImGui::DragFloat3("Angular Velocity", &sceneObj.AngularVelocity.x, 0.01f);
            if (ImGui::Button("Reset Rotation")) {
                sceneObj.ResetRotation();
            }

            ImGui::Separator();

            for (size_t j = 0; j < sceneObj.GetGpuObjectCount(); ++j) {
                ImGui::PushID(j);
                GPUobject& gpuObj = sceneObj.GetGpuObject(j);
                
                std::string gpu_label = (gpuObj.type == 0) ? "Sphere Data" : "Ring Data";
                if(ImGui::TreeNode(gpu_label.c_str())) {
                     ImGui::DragFloat("Radius 1", &gpuObj.r1, 0.05f, 0.0f);
                     if (gpuObj.type == 1) { 
                         ImGui::DragFloat("Radius 2 (Inner)", &gpuObj.r2, 0.05f, 0.0f);
                     }
                     ImGui::Separator();
                     ImGui::Text("Material:");
                     ImGui::ColorEdit3("Albedo", &gpuObj.m.albedo.x);
                     ImGui::InputInt("Texture ID", &gpuObj.m.textureID, 1, 10);
                     ImGui::SliderFloat("Metallic", &gpuObj.m.metallic, 0.0f, 1.0f);
                     ImGui::SliderFloat("Roughness", &gpuObj.m.roughness, 0.0f, 1.0f);
                     ImGui::DragFloat("Emission", &gpuObj.m.emission, 10.0f, 0.0f, 50000.0f);
                     ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    ImGui::End();
}

void Application::save_scene_to_file(const std::string& filename) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return;
    }

    outfile << sceneObjects.size() << std::endl;

    for (const auto& sceneObj_ptr : sceneObjects) {
        const SceneObject& sceneObj = *sceneObj_ptr;

        std::string name_to_save = sceneObj.Name;
        std::replace(name_to_save.begin(), name_to_save.end(), ' ', '_'); 

        outfile << static_cast<int>(sceneObj.Type) << " ";
        outfile << name_to_save << " "; 
        outfile << sceneObj.Mass << " ";
        outfile << sceneObj.GetPosition().x << " " << sceneObj.GetPosition().y << " " << sceneObj.GetPosition().z << " ";
        outfile << sceneObj.velocity.x << " " << sceneObj.velocity.y << " " << sceneObj.velocity.z << " ";
        outfile << sceneObj.Orientation.x << " " << sceneObj.Orientation.y << " " << sceneObj.Orientation.z << " " << sceneObj.Orientation.w << " ";
        outfile << sceneObj.AngularVelocity.x << " " << sceneObj.AngularVelocity.y << " " << sceneObj.AngularVelocity.z << " ";
        outfile << (sceneObj.hasRings ? 1 : 0) << std::endl;
        
        for (size_t i = 0; i < sceneObj.GetGpuObjectCount(); ++i) {
            const GPUobject& gpuObj = sceneObj.GetGpuObject(i);
            outfile << gpuObj.r1 << " " << gpuObj.r2 << " ";
            outfile << gpuObj.m.albedo.x << " " << gpuObj.m.albedo.y << " " << gpuObj.m.albedo.z << " ";
            outfile << gpuObj.m.emission << " " << gpuObj.m.metallic << " " << gpuObj.m.roughness << " " << gpuObj.m.textureID << std::endl;
        }
        outfile << "---" << std::endl;
    }
    outfile.close();
    std::cout << "Scene saved to " << filename << std::endl;
}

void Application::load_scene_from_file(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << filename << std::endl;
        return;
    }

    sceneObjects.clear(); 

    size_t object_count;
    infile >> object_count;
    if (infile.fail()) return;

    for (size_t i = 0; i < object_count; ++i) {
        int type_int;
        infile >> type_int;
        ObjectType type = static_cast<ObjectType>(type_int);

        auto new_scene_object = std::make_unique<SceneObject>(type, vec3(0.0f), 0.0f);
        SceneObject& sceneObj = *new_scene_object;
    
        std::string name_from_file;
        infile >> name_from_file; 
        std::replace(name_from_file.begin(), name_from_file.end(), '_', ' '); 
        sceneObj.Name = name_from_file; 

        vec3 loadedPosition;
        int rings_int;

        infile >> sceneObj.Mass;
        infile >> loadedPosition.x >> loadedPosition.y >> loadedPosition.z;
        infile >> sceneObj.velocity.x >> sceneObj.velocity.y >> sceneObj.velocity.z;
        infile >> sceneObj.Orientation.x >> sceneObj.Orientation.y >> sceneObj.Orientation.z >> sceneObj.Orientation.w;
        infile >> sceneObj.AngularVelocity.x >> sceneObj.AngularVelocity.y >> sceneObj.AngularVelocity.z;
        infile >> rings_int;
        
        bool shouldHaveRings = (rings_int == 1);
        if (sceneObj.hasRings != shouldHaveRings) {
            sceneObj.hasRings = shouldHaveRings;
            sceneObj.SetupAs(type);
        }

        for (size_t j = 0; j < sceneObj.GetGpuObjectCount(); ++j) {
            GPUobject& gpuObj = sceneObj.GetGpuObject(j);
            infile >> gpuObj.r1 >> gpuObj.r2;
            infile >> gpuObj.m.albedo.x >> gpuObj.m.albedo.y >> gpuObj.m.albedo.z;
            infile >> gpuObj.m.emission >> gpuObj.m.metallic >> gpuObj.m.roughness >> gpuObj.m.textureID;
        }
        
        sceneObj.SetPosition(loadedPosition);
        for (auto& gpu_obj : sceneObj.gpuObjects) {
             gpu_obj.rot_quat = sceneObj.Orientation;
        }
        
        std::string separator;
        infile >> separator; 

        sceneObjects.push_back(std::move(new_scene_object));
    }

    infile.close();
    frame_acc_count = 1;
    init_trails();
    std::cout << "Scene loaded from " << filename << ". Total objects: " << sceneObjects.size() << std::endl;
}

void Application::scan_for_save_files() {
    saveFiles.clear();
    std::string path = "./saves";
    

    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directory(path);
        std::cout << "Created ./saves/ directory." << std::endl;
        return;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".scene") {
            saveFiles.push_back(entry.path().filename().string());
        }
    }
    std::cout << "Found " << saveFiles.size() << " save files." << std::endl;
}

vec3 Application::calculate_orbital_velocity(float parentMass, float newObjectMass, vec3 directionToNew, float distance, float eccentricity, float inclination) const {

    float totalMass = parentMass;
    if (totalMass <= 0.0f) 
        return vec3(0.0f);

    float semiMajorAxis = distance / (1.0f - eccentricity + 1e-6f);
    totalMass += newObjectMass;
    float speedSq = gravitationalConstant * totalMass * ((2.0f / distance) - (1.0f / semiMajorAxis));
    
    if (speedSq < 0) {
        std::cerr << "Warning: Requested orbit is unstable (hyperbolic). Setting initial velocity to 0." << std::endl;
        return vec3(0.0f);
    }

    float speed = sqrt(speedSq);

    vec3 up_vec = vec3(0.0, 1.0, 0.0); 
    vec3 flat_velocity_dir = normalize(cross(directionToNew, up_vec));

    vec3 inclination_axis = directionToNew;

    vec4 inclination_quat = quat_from_axis_angle(inclination_axis, DegreesToRadians * inclination);
    vec3 final_velocity_dir = rotate(inclination_quat, flat_velocity_dir);

    return final_velocity_dir * speed;
}

void Application::add_object(ObjectType type, float mass, float distance, float eccentricity, float inclination) {
    float parentMass = 0.0f;
    vec3 parentVelocity = vec3(0.0f);
    vec3 parentPosition = vec3(0.0f);
    SceneObject* parentObject = nullptr;

    if (sceneObjects.empty()) {
        selectedObjectIndex = -1; 
    }

    if (selectedObjectIndex >= 0 && selectedObjectIndex < sceneObjects.size()) {
        parentObject = sceneObjects[selectedObjectIndex].get();
        parentMass = parentObject->Mass;
        parentVelocity = parentObject->velocity;
        parentPosition = parentObject->GetPosition();
    }


    float randomAngle = (rand() / (float)RAND_MAX) * 2.0f * M_PI;
    vec3 directionOnPlane = normalize(vec3(cos(randomAngle), 0.0f, sin(randomAngle)));

    vec3 initialPosition = parentPosition + directionOnPlane * distance;
    vec3 relativeOrbitalVel = calculate_orbital_velocity(parentMass, mass, directionOnPlane, distance, eccentricity, inclination);
    vec3 initialVelocity = parentVelocity + relativeOrbitalVel;
    
    auto new_obj = std::make_unique<SceneObject>(type, initialPosition, mass);
    new_obj->velocity = initialVelocity;
    
    sceneObjects.push_back(std::move(new_obj));
    frame_acc_count = 1;
    init_trails();
}

void Application::delete_object(int obj_index) {
    if (obj_index < 0 || obj_index >= sceneObjects.size()) {
        std::cerr << "Error: Invalid index for object deletion." << std::endl;
        return;
    }
    sceneObjects.erase(sceneObjects.begin() + obj_index);

    frame_acc_count = 1;
    init_trails();

    std::cout << "Deleted object. Total scene objects: " << sceneObjects.size() << std::endl;
}

void Application::init_trails() {
    cleanup_trails();
    trailRenderers.resize(sceneObjects.size());

    for (int i = 0; i < sceneObjects.size(); ++i) {
        glGenVertexArrays(1, &trailRenderers[i].vao);
        glGenBuffers(1, &trailRenderers[i].vbo);

        glBindVertexArray(trailRenderers[i].vao);
        glBindBuffer(GL_ARRAY_BUFFER, trailRenderers[i].vbo);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3) + sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vec3) + sizeof(float), (void*)sizeof(vec3));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }
}

void Application::cleanup_trails() {
    for (auto& trail : trailRenderers) {
        if(trail.vbo) glDeleteBuffers(1, &trail.vbo);
        if(trail.vao) glDeleteVertexArrays(1, &trail.vao);
    }
    trailRenderers.clear();
}

void Application::update_trails(const vec3& centerOfMass) {
    if (sceneObjects.size() != trailRenderers.size()) 
        init_trails();
    
    struct TrailVertex {
        vec3 pos;
        float age;
    };

    float totalMass = 0.0f;
    vec3 weightedVelocitySum(0.0f, 0.0f, 0.0f);

    for (const auto& obj_ptr : sceneObjects) {
        weightedVelocitySum += obj_ptr->velocity * obj_ptr->Mass;
        totalMass += obj_ptr->Mass;
    }


    for (int i = 0; i < sceneObjects.size(); ++i) {
        SceneObject& obj = *sceneObjects[i];

        float speed = length(obj.velocity - weightedVelocitySum / totalMass);

        obj.MaxTrailPoints = static_cast<size_t>(30000 / (timeScale * (std::pow(speed, 2) + 1)));
        obj.TrailPoints.push_front(obj.GetPosition() - centerOfMass);

        while (obj.TrailPoints.size() > obj.MaxTrailPoints) {
            obj.TrailPoints.pop_back();
        }

        std::vector<TrailVertex> vertices;
        vertices.reserve(obj.TrailPoints.size());
        for (size_t j = 0; j < obj.TrailPoints.size(); ++j) {
            float age = (obj.MaxTrailPoints > 0) ? static_cast<float>(j) / static_cast<float>(obj.MaxTrailPoints) : 0.0f;
            vertices.push_back({ obj.TrailPoints[j] + centerOfMass, age });
        }
        trailRenderers[i].pointCount = vertices.size();

        if (trailRenderers[i].pointCount > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, trailRenderers[i].vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TrailVertex), vertices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

void Application::render_trails() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(trailShader);

    int lastWrittenAccIndex = 1 - curr_acc_index; 

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accTex[lastWrittenAccIndex * 5 + 3]);
    glUniform1i(glGetUniformLocation(trailShader, "gbufferDepth"), 0);

    mat4 view = LookAt(camera->Position, camera->Target, vec3(0.0, 1.0, 0.0));
    mat4 projection = Perspective(camera->Fov, (float)fbWidth / (float)fbHeight, 0.01f, 1.0e10f);
    mat4 mvp = projection * view;
    
    glUniformMatrix4fv(glGetUniformLocation(trailShader, "mvp"), 1, GL_TRUE, mvp);
    
    for (int i = 0; i < trailRenderers.size(); ++i) {

        if (i >= sceneObjects.size() || i >= trailRenderers.size()) continue;

        const SceneObject& sceneObj = *sceneObjects[i];
        vec3 color = sceneObj.GetGpuObject(0).m.albedo;
        glUniform3fv(glGetUniformLocation(trailShader, "trailColor"), 1, pow((color + vec3(0.1f)) / 1.1f, 0.25));

        float thickness = 1.0f + log10(std::max(1.0f, sceneObj.Mass)) * 1.5f;
        thickness = std::min(thickness, 7.0f); 
        glLineWidth(thickness);


        if (trailRenderers[i].pointCount > 1) {
            glBindVertexArray(trailRenderers[i].vao);
            glDrawArrays(GL_LINE_STRIP, 0, trailRenderers[i].pointCount);
        }
    }

    glLineWidth(1.0f); 
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}


// --- Static GLFW Callbacks (Forward to member functions) ---
void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->M_KeyCallback(key, scancode, action, mods);
}
void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->M_MouseButtonCallback(button, action, mods);
}
void Application::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->M_CursorPosCallback(xpos, ypos);
}
void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->M_ScrollCallback(xoffset, yoffset);
}
void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->M_FramebufferSizeCallback(width, height);
}

// --- Member Callback Implementations (Largely the same as previous version) ---
void Application::M_KeyCallback(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
                camera->Fov -= 2.0f;
                if (camera->Fov < 10.0f) camera->Fov = 10.0f;
                frame_acc_count = 1;
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
                camera->Fov += 2.0f;
                if (camera->Fov > 120.0f) camera->Fov = 120.0f;
                frame_acc_count = 1;
                break;
            case GLFW_KEY_LEFT:
                selectedObjectIndex = (selectedObjectIndex - 1) % sceneObjects.size();
                frame_acc_count = 1;
                break;
            case GLFW_KEY_RIGHT:
                selectedObjectIndex = (selectedObjectIndex + 1) % sceneObjects.size();
                frame_acc_count = 1;
                break;
            case GLFW_KEY_UP:
            case GLFW_KEY_DOWN:
                selectedObjectIndex = -1;
                frame_acc_count = 1;
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
        }
    }
}
void Application::M_MouseButtonCallback(int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (action == GLFW_PRESS) {
        isOrbiting = true;
    } else if (action == GLFW_RELEASE) {
        isOrbiting = false;
    }
}
void Application::M_CursorPosCallback(double xpos, double ypos) {
    static double lastX = xpos;
    static double lastY = ypos;

    if (isOrbiting) {
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;

        camera->ProcessOrbitDrag(xoffset, yoffset);
        frame_acc_count = 1;
    }

    lastX = xpos;
    lastY = ypos;
}
void Application::M_ScrollCallback(double xoffset, double yoffset) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    camera->ProcessOrbitZoom(yoffset);
    frame_acc_count = 1;
}
void Application::M_FramebufferSizeCallback(int width, int height) {

    fbWidth = width;
    fbHeight = height;

    renderWidth = static_cast<int>(static_cast<float>(fbWidth) * resScale);
    renderHeight = static_cast<int>(static_cast<float>(fbHeight) * resScale);

    glDeleteTextures(10, accTex);
    glDeleteFramebuffers(2, accFBO);
    
    for (const auto& mip : bloomMipChain) {
        glDeleteFramebuffers(1, &mip.fbo);
        glDeleteTextures(1, &mip.texture);
        glDeleteFramebuffers(1, &mip.pingpong_fbo);
        glDeleteTextures(1, &mip.pingpong_texture);
    }
    bloomMipChain.clear();

    glDeleteFramebuffers(2, denoiseFBO);
    glDeleteTextures(2, denoiseTex);

    init_framebuffers();
}
