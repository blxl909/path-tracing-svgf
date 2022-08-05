#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>
#include <ctime>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "lib/hdrloader.h"
#include "lib/stb_image.h"
#include "lib/filesystem.h"

#include "Utils/camera.h"
#include "Utils/PointLight.h"
#include "Utils/gui_config.h"
#include "Utils/BVH.h"
#include "Utils/render_pass.h"
#include "Utils/obj_loader.h"
#include "Utils/shader.h"
#include "Utils/help_func.h"
#include "Utils/hdr_compute.h"

using namespace glm;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

Camera camera;

mat4 pre_viewproj = camera.cam_proj_mat * camera.cam_view_mat;

GLuint trianglesTextureBuffer;
GLuint nodesTextureBuffer;
GLuint pointLightBuffer;

GLuint hdrMap;
GLuint hdrCache;
