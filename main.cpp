#include "main.h"


void cursor_position_callback(GLFWwindow* window, double x, double y);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);


GLuint init_normal_depth;
GLuint init_world_position;
GLuint init_velocity;
GLuint init_fwidth;

GLuint curColor;
GLuint Emission;
GLuint Albedo;

GLuint reprojectedColor;
GLuint reprojectedmomenthistory;

GLuint variance_filtered_color;

GLuint atrous_flitered_color0;//swap buffer

GLuint tmp_atrous_result;

GLuint next_frame_color_input;//same as lastColor 

GLuint taa_output;


GLuint lastColor;//atrous 的一级输出
GLuint lastNormalDepth;
GLuint lastMomentHistory;
GLuint last_color_taa;

RenderPass pass_path_tracing;
RenderPass pass2;
RenderPass pass_moment_filter;
RenderPass pass_Atrous_filter;//save info pass
RenderPass bilt_pass;
RenderPass save_next_frame_pass;
RenderPass pass_pre_info;
RenderPass pass_taa;
RenderPass pass3;
NormalRenderPass init_pass;





mat4 pre_viewproj = camera.cam_proj_mat * camera.cam_view_mat;

int main(int argc, char** argv)
{
	const char* glsl_version = "#version 450";
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GPU_pathtracer", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);




	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;


	// Setup Dear ImGui style
	ImGui::StyleColorsDark();


	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);


	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);


	std::vector<Triangle> triangles;



	int objIndex = 0;
	std::vector<float> vertices;
	// when number is under 0.0, that means we use texture to define it 
	Material m_clock;
	m_clock.roughness = -1.0;
	m_clock.specular = 0.0;//1.0
	m_clock.metallic = -1.0;//1.0
	m_clock.clearcoat = 0.0;//1.0
	m_clock.clearcoatGloss = 0.0;
	m_clock.baseColor = vec3(-1, -1, -1);
	
	readObj("models/clock.obj", vertices, triangles, m_clock, getTransformMatrix(vec3(0, 0, 0), vec3(0, 0, 0), vec3(1, 1, 1)), true, objIndex++);


	/*m.roughness = -1.0;
	m.metallic =-1.0;
	m.specular = 0.0;
	m.baseColor = vec3(-1, -1, -1);
	readObj("models/plant.obj", vertices, triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(-0.75, -0.5, 0), vec3(2, 2, 2)), false, objIndex++);*/

	int nTriangles = triangles.size();
	std::cout << "模型读取完成: 共 " << nTriangles << " 个三角形" << std::endl;

	// 建立 bvh
	BVHNode testNode;
	testNode.left = 255;
	testNode.right = 128;
	testNode.n = 30;
	testNode.AA = vec3(1, 1, 0);
	testNode.BB = vec3(0, 1, 0);
	std::vector<BVHNode> nodes{ testNode };

	buildBVHwithSAH(triangles, nodes, 0, triangles.size() - 1, 8);
	int nNodes = nodes.size();
	std::cout << "BVH 建立完成: 共 " << nNodes << " 个节点" << std::endl;

	// 编码 三角形, 材质
	std::vector<Triangle_encoded> triangles_encoded(nTriangles);
	for (int i = 0; i < nTriangles; i++) {
		Triangle& t = triangles[i];
		Material& m = t.material;
		// 顶点位置
		triangles_encoded[i].p1 = t.p1;
		triangles_encoded[i].p2 = t.p2;
		triangles_encoded[i].p3 = t.p3;
		// 顶点法线
		triangles_encoded[i].n1 = t.n1;
		triangles_encoded[i].n2 = t.n2;
		triangles_encoded[i].n3 = t.n3;
		// 材质
		triangles_encoded[i].emissive = m.emissive;
		triangles_encoded[i].baseColor = m.baseColor;
		triangles_encoded[i].param1 = vec3(m.subsurface, m.metallic, m.specular);
		triangles_encoded[i].param2 = vec3(m.specularTint, m.roughness, m.anisotropic);
		triangles_encoded[i].param3 = vec3(m.sheen, m.sheenTint, m.clearcoat);
		triangles_encoded[i].param4 = vec3(m.clearcoatGloss, m.IOR, m.transmission);

		triangles_encoded[i].uvPacked1 = vec3(t.uv1, t.uv2.x);
		triangles_encoded[i].uvPacked2 = vec3(t.uv2.y, t.uv3);
		triangles_encoded[i].objIndex = vec3((float)t.objID, 0.0, 0.0);
	}

	// 编码 BVHNode, aabb
	std::vector<BVHNode_encoded> nodes_encoded(nNodes);
	for (int i = 0; i < nNodes; i++) {
		nodes_encoded[i].childs = vec3(nodes[i].left, nodes[i].right, 0);
		nodes_encoded[i].leafInfo = vec3(nodes[i].n, nodes[i].index, 0);
		nodes_encoded[i].AA = nodes[i].AA;
		nodes_encoded[i].BB = nodes[i].BB;
	}



	// 生成纹理

	// 三角形数组
	GLuint tbo0;
	glGenBuffers(1, &tbo0);
	glBindBuffer(GL_TEXTURE_BUFFER, tbo0);
	glBufferData(GL_TEXTURE_BUFFER, triangles_encoded.size() * sizeof(Triangle_encoded), &triangles_encoded[0], GL_STATIC_DRAW);
	glGenTextures(1, &trianglesTextureBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, trianglesTextureBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbo0);

	// BVHNode 数组
	GLuint tbo1;
	glGenBuffers(1, &tbo1);
	glBindBuffer(GL_TEXTURE_BUFFER, tbo1);
	glBufferData(GL_TEXTURE_BUFFER, nodes_encoded.size() * sizeof(BVHNode_encoded), &nodes_encoded[0], GL_STATIC_DRAW);
	glGenTextures(1, &nodesTextureBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, nodesTextureBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbo1);

	std::vector<PointLight> pointLights;



	pointLights.push_back(PointLight(vec3(0.5, 0.5, 0.5), vec3(10, 10, 10)));
	pointLights.push_back(PointLight(vec3(-0.5, 0.75, 0.5), vec3(8, 4, 4)));
	pointLights.push_back(PointLight(vec3(-0.5, 0.75, 0.75), vec3(0, 3, 4)));
	pointLights.push_back(PointLight(vec3(0.75, 0.75, 0.75), vec3(12, 3, 4)));
	//point light tbo
	GLuint tbo2;
	glGenBuffers(1, &tbo2);
	glBindBuffer(GL_TEXTURE_BUFFER, tbo2);
	glBufferData(GL_TEXTURE_BUFFER, pointLights.size() * sizeof(PointLight), &pointLights[0], GL_STATIC_DRAW);
	glGenTextures(1, &pointLightBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, pointLightBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbo2);

	// hdr 全景图
	HDRLoaderResult hdrRes;
	bool r = HDRLoader::load("./HDR/chinese_garden_2k.hdr", hdrRes);
	hdrMap = getTextureRGB32F(hdrRes.width, hdrRes.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, hdrRes.width, hdrRes.height, 0, GL_RGB, GL_FLOAT, hdrRes.cols);

	// hdr 重要性采样 cache
	std::cout << "计算 HDR 贴图重要性采样 Cache, 当前分辨率: " << hdrRes.width << " " << hdrRes.height << std::endl;
	float* cache = calculateHdrCache(hdrRes.cols, hdrRes.width, hdrRes.height);
	hdrCache = getTextureRGB32F(hdrRes.width, hdrRes.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, hdrRes.width, hdrRes.height, 0, GL_RGB, GL_FLOAT, cache);
	hdrResolution = hdrRes.width;

	//----------------------
		//load material

	GLuint materials_array;
	glGenTextures(1, &materials_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, materials_array);
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	//total 12 textures
	//hard code width height, maybe change later
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4096, 4096, 12);
	int slot_index = 0;
	load_texture_to_material_array("Textures/clock_albedo.bmp", slot_index++);
	load_texture_to_material_array("Textures/clock_metallic.bmp", slot_index++);
	load_texture_to_material_array("Textures/clock_normal.bmp", slot_index++);
	load_texture_to_material_array("Textures/clock_roughness.bmp", slot_index++);

	load_texture_to_material_array("Textures/plant_albedo.bmp", slot_index++);
	load_texture_to_material_array("Textures/plant_metallic.bmp", slot_index++);
	load_texture_to_material_array("Textures/plant_normal.bmp", slot_index++);
	load_texture_to_material_array("Textures/plant_roughness.bmp", slot_index++);



	//----------------------

	pass3.program = getShaderProgram("./shaders/pass3.frag", "./shaders/vshader.vsh");
	pass3.bindData(true);
	//-----------------------
	init_pass.program = getShaderProgram("./shaders/normalfshader.frag", "./shaders/normalvshader.vsh");

	init_world_position = getTextureRGB32F(init_pass.width, init_pass.height);
	init_normal_depth = getTextureRGB32F(init_pass.width, init_pass.height);
	init_velocity = getTextureRGB32F(init_pass.width, init_pass.height);
	init_fwidth = getTextureRGB32F(init_pass.width, init_pass.height);

	init_pass.colorAttachments.push_back(init_world_position);
	init_pass.colorAttachments.push_back(init_normal_depth);
	init_pass.colorAttachments.push_back(init_velocity);
	init_pass.colorAttachments.push_back(init_fwidth);

	init_pass.bindData(vertices);

	glUseProgram(init_pass.program);
	glUniform1i(glGetUniformLocation(init_pass.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(init_pass.program, "screen_height"), SCR_HEIGHT);
	glUseProgram(0);

	//--------------------
	pass_path_tracing.program = getShaderProgram("./shaders/path_tracing.frag", "./shaders/vshader.vsh");

	curColor = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);
	Emission = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);
	Albedo = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);


	pass_path_tracing.colorAttachments.push_back(curColor);
	pass_path_tracing.colorAttachments.push_back(Emission);
	pass_path_tracing.colorAttachments.push_back(Albedo);


	pass_path_tracing.bindData(false);

	glUseProgram(pass_path_tracing.program);
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "nTriangles"), triangles.size());
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "nNodes"), nodes.size());
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "width"), pass_path_tracing.width);
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "height"), pass_path_tracing.height);
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "pointLightSize"), pointLights.size());
	glUseProgram(0);

	

	//----------------------------
	bilt_pass.program = getShaderProgram("./shaders/bilt.frag", "./shaders/vshader.vsh");
	tmp_atrous_result = getTextureRGB32F(bilt_pass.width, bilt_pass.height);
	bilt_pass.colorAttachments.push_back(tmp_atrous_result);
	bilt_pass.bindData(false);
	//----------------------------
	save_next_frame_pass.program = getShaderProgram("./shaders/bilt.frag", "./shaders/vshader.vsh");
	next_frame_color_input = getTextureRGB32F(save_next_frame_pass.width, save_next_frame_pass.height);
	save_next_frame_pass.colorAttachments.push_back(next_frame_color_input);
	save_next_frame_pass.bindData(false);
	//----------------------------
	pass_taa.program = getShaderProgram("./shaders/taa.frag", "./shaders/vshader.vsh");
	taa_output = getTextureRGB32F(pass_taa.width, pass_taa.height);
	pass_taa.colorAttachments.push_back(taa_output);
	pass_taa.bindData(false);

	glUseProgram(pass_taa.program);
	glUniform1i(glGetUniformLocation(pass_taa.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(pass_taa.program, "screen_height"), SCR_HEIGHT);
	glUseProgram(0);
	//----------------------------------------
	//----------------------------------------

	RenderPass next_frame_input;

	next_frame_input.program = getShaderProgram("./shaders/save_frame_data.frag", "./shaders/vshader.vsh");

	GLuint lastIllumination = getTextureRGB32F(next_frame_input.width, next_frame_input.height);
	GLuint last_normal_depth = getTextureRGB32F(next_frame_input.width, next_frame_input.height);
	GLuint last_Moments_HistoryLength = getTextureRGB32F(next_frame_input.width, next_frame_input.height);
	GLuint last_acc_color = getTextureRGB32F(next_frame_input.width, next_frame_input.height);
	next_frame_input.colorAttachments.push_back(lastIllumination);
	next_frame_input.colorAttachments.push_back(last_normal_depth);
	next_frame_input.colorAttachments.push_back(last_Moments_HistoryLength);
	next_frame_input.colorAttachments.push_back(last_acc_color);

	next_frame_input.bindData(false);



	//----------------------------------------
	RenderPass new_project_pass;
	new_project_pass.program = getShaderProgram("./shaders/svgf_reproject.frag", "./shaders/vshader.vsh");
	GLuint curIllumination = getTextureRGB32F(new_project_pass.width, new_project_pass.height);
	GLuint curMomentHistory = getTextureRGB32F(new_project_pass.width, new_project_pass.height);

	new_project_pass.colorAttachments.push_back(curIllumination);
	new_project_pass.colorAttachments.push_back(curMomentHistory);

	new_project_pass.bindData(false);

	new_project_pass.set_uniform_float("inv_screen_width", 1.0 / camera.width);
	new_project_pass.set_uniform_float("inv_screen_height", 1.0 / camera.height);

	//------------------------------------
	RenderPass new_variance_pass;
	new_variance_pass.program = getShaderProgram("./shaders/svgf_variance.frag", "./shaders/vshader.vsh");
	GLuint variance_compute_illumination = getTextureRGB32F(new_variance_pass.width, new_variance_pass.height);
	new_variance_pass.colorAttachments.push_back(variance_compute_illumination);
	new_variance_pass.bindData(false);

	new_variance_pass.set_uniform_float("inv_screen_width", 1.0 / camera.width);
	new_variance_pass.set_uniform_float("inv_screen_height", 1.0 / camera.height);

	//------------------------------------
	RenderPass new_atrous_pass;
	new_atrous_pass.program = getShaderProgram("./shaders/svgf_Atrous.frag", "./shaders/vshader.vsh");
	GLuint atrous_output = getTextureRGB32F(new_atrous_pass.width, new_atrous_pass.height);
	new_atrous_pass.colorAttachments.push_back(atrous_output);
	new_atrous_pass.bindData(false);

	new_atrous_pass.set_uniform_float("inv_screen_width", 1.0 / camera.width);
	new_atrous_pass.set_uniform_float("inv_screen_height", 1.0 / camera.height);

	//-----------------------------------
	RenderPass svgf_modulate_pass;
	svgf_modulate_pass.program = getShaderProgram("./shaders/svgf_modulate.frag", "./shaders/vshader.vsh");
	GLuint modulate_color = getTextureRGB32F(svgf_modulate_pass.width, svgf_modulate_pass.height);
	svgf_modulate_pass.colorAttachments.push_back(modulate_color);

	svgf_modulate_pass.bindData(false);

	//------------------------------------
	RenderPass output_pass;
	output_pass.program = getShaderProgram("./shaders/pass3.frag", "./shaders/vshader.vsh");
	output_pass.bindData(true);


	//----------------------------------------
	//----------------------------------------
	//----------------------------
	bool use_normal_texture = false;
	debug_gui debug_window;
	parameter_config config;

	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		if (slot_index % 4 != 0) {
			std::cout << "currently only support 4 textures each obj object, and should be set all of them" << std::endl;
			break;
		}
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;


		camera.update(window, deltaTime);

		glm::mat4 view = camera.cam_view_mat;
		glm::mat4 projection = camera.cam_proj_mat;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
			ImGui::Begin("parameter settings");
			ImGui::Text("control parameter during passes.");

			/*ImGui::SliderFloat("reporject_depth_threshold", &config.reproj_depth_threshold, 0.0f, 1.0f);
			ImGui::SliderFloat("reporject_normal_threshold", &config.reproj_normal_threshold, 0.0f, 1.0f);*/

			ImGui::SliderFloat("color_clamp_threshold", &config.clamp_threshold, 1.0f, 10.0f);
			ImGui::SliderInt("max_tracing_depth", &config.max_tracing_depth, 1, 6);

			//ImGui::SliderFloat("sigma_z", &config.sigma_z, 0.1f, 5.0f);
			ImGui::Text("sigma_z is calculated automatically in shader.");
			ImGui::SliderFloat("sigma_n", &config.sigma_n, 50.0f, 200.0f);
			ImGui::SliderFloat("sigma_l", &config.sigma_l, 1.0f, 10.0f);

			ImGui::SliderInt("num_atrous_iterations", &config.num_atrous_iterations, 2, 8);

			ImGui::End();
		}


		{

			ImGui::Begin("show pass image");                          // Create a window called "Hello, world!" and append into it.
			ImGui::Text("pick a pass to show the output.");               // Display some text (you can use a format strings too)

			if (ImGui::Button("path_tracing_pic_1spp")) {
				camera.frameCounter = 0;
				debug_window.update_debug_state(debug_view_type::path_tracing_pic_1spp);
			}
			if (ImGui::Button("svgf_reprojected_pic")) {
				camera.frameCounter = 0;
				debug_window.update_debug_state(debug_view_type::svgf_reprojected_pic);
			}
			if (ImGui::Button("final_pic")) {
				camera.frameCounter = 0;
				debug_window.update_debug_state(debug_view_type::final_pic);
			}
			if (ImGui::Button("accumulate_color")) {
				camera.frameCounter = 0;
				debug_window.update_debug_state(debug_view_type::accumulate_color);
			}
			ImGui::Text("normal texture will only have meaning when actually have normal texture");
			if (ImGui::Checkbox("use_normal_texture", &use_normal_texture)) {
				camera.frameCounter = 0;
			}
			

			//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			//ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			//ImGui::SameLine();
			//ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Text("iterations: %u", camera.frameCounter);
			ImGui::End();
		}




		glUseProgram(init_pass.program);
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "view"), 1, GL_FALSE, value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "projection"), 1, GL_FALSE, value_ptr(projection));

		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "pre_viewproj"), 1, GL_FALSE, value_ptr(pre_viewproj));
		glUniform1ui(glGetUniformLocation(init_pass.program, "frameCounter"), camera.frameCounter);

		init_pass.draw();
		//-------------------------------------------
		mat4 cameraRotate = inverse(camera.cam_view_mat);

		pass_path_tracing.set_uniform_vec3("eye", camera.cam_position);
		pass_path_tracing.set_uniform_mat4("cameraRotate", cameraRotate);
		pass_path_tracing.set_uniform_uint("frameCounter", camera.frameCounter);
		pass_path_tracing.set_uniform_int("hdrResolution", hdrResolution);
		pass_path_tracing.set_uniform_bool("use_normal_map", use_normal_texture);
		pass_path_tracing.set_uniform_bool("accumulate", debug_window.accumulate_color);
		pass_path_tracing.set_uniform_float("clamp_threshold", config.clamp_threshold);
		pass_path_tracing.set_uniform_int("max_tracing_depth", config.max_tracing_depth);

		pass_path_tracing.reset_texture_slot();
		pass_path_tracing.set_texture_uniform(GL_TEXTURE_BUFFER, trianglesTextureBuffer, "triangles");
		pass_path_tracing.set_texture_uniform(GL_TEXTURE_BUFFER, nodesTextureBuffer, "nodes");

		if (debug_window.accumulate_color) {
			pass_path_tracing.set_texture_uniform(GL_TEXTURE_2D, last_acc_color, "lastFrame");
		}

		pass_path_tracing.set_texture_uniform(GL_TEXTURE_2D, hdrMap, "hdrMap");
		pass_path_tracing.set_texture_uniform(GL_TEXTURE_2D, hdrCache, "hdrCache");
		pass_path_tracing.set_texture_uniform(GL_TEXTURE_2D_ARRAY, materials_array, "material_array");
		pass_path_tracing.set_texture_uniform(GL_TEXTURE_BUFFER, pointLightBuffer, "pointLights");


		pass_path_tracing.draw();


		//------------------------------
		new_project_pass.reset_texture_slot();
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, init_velocity, "gMotion");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, curColor, "gColor");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, Albedo, "gAlbedo");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, Emission, "gEmission");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, lastIllumination, "gPrevIllum");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, last_Moments_HistoryLength, "gPrevMoments_HistoryLength");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, init_normal_depth, "gNormalAndLinearZ");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, last_normal_depth, "gPrevNormalAndLinearZ");
		new_project_pass.set_texture_uniform(GL_TEXTURE_2D, init_fwidth, "gNormalDepthFwidth");
		new_project_pass.draw();
		//-------------------------------
		new_variance_pass.reset_texture_slot();
		new_variance_pass.set_uniform_float("gPhiColor", config.sigma_l);
		new_variance_pass.set_uniform_float("gPhiNormal", config.sigma_n);
		new_variance_pass.set_texture_uniform(GL_TEXTURE_2D, curIllumination, "gIllumination");
		new_variance_pass.set_texture_uniform(GL_TEXTURE_2D, curMomentHistory, "gMoments_HistoryLength");
		new_variance_pass.set_texture_uniform(GL_TEXTURE_2D, init_normal_depth, "gNormalAndLinearZ");
		new_variance_pass.set_texture_uniform(GL_TEXTURE_2D, init_fwidth, "gNormalDepthFwidth");
		new_variance_pass.draw();

		//-------------------------------

		for (int i = 0; i < config.num_atrous_iterations; i++) {
			new_atrous_pass.reset_texture_slot();

			new_atrous_pass.set_uniform_float("gPhiColor", config.sigma_l);
			new_atrous_pass.set_uniform_float("gPhiNormal", config.sigma_n);
			new_atrous_pass.set_uniform_int("gStepSize", 1 << i);

			new_atrous_pass.set_texture_uniform(GL_TEXTURE_2D, init_normal_depth, "gNormalAndLinearZ");
			new_atrous_pass.set_texture_uniform(GL_TEXTURE_2D, init_fwidth, "gNormalDepthFwidth");

			if (i == 0) {
				new_atrous_pass.set_texture_uniform(GL_TEXTURE_2D, variance_compute_illumination, "gIllumination");
			}
			else {
				new_atrous_pass.set_texture_uniform(GL_TEXTURE_2D, tmp_atrous_result, "gIllumination");
			}
			new_atrous_pass.draw();

			bilt_pass.reset_texture_slot();
			bilt_pass.set_texture_uniform(GL_TEXTURE_2D, atrous_output, "in_texture");
			bilt_pass.draw();

			if (i == 1) {
				save_next_frame_pass.reset_texture_slot();
				save_next_frame_pass.set_texture_uniform(GL_TEXTURE_2D, atrous_output, "in_texture");
				save_next_frame_pass.draw();
			}
		}


		//-------------------------------
		next_frame_input.reset_texture_slot();
		next_frame_input.set_texture_uniform(GL_TEXTURE_2D, next_frame_color_input, "texPass0");
		next_frame_input.set_texture_uniform(GL_TEXTURE_2D, init_normal_depth, "texPass1");
		next_frame_input.set_texture_uniform(GL_TEXTURE_2D, curMomentHistory, "texPass2");
		next_frame_input.set_texture_uniform(GL_TEXTURE_2D, curColor, "accColor");

		next_frame_input.draw();
		//-----------------------------------
		svgf_modulate_pass.reset_texture_slot();
		svgf_modulate_pass.set_texture_uniform(GL_TEXTURE_2D, Albedo, "gAlbedo");
		svgf_modulate_pass.set_texture_uniform(GL_TEXTURE_2D, Emission, "gEmission");
		svgf_modulate_pass.set_texture_uniform(GL_TEXTURE_2D, atrous_output, "gIllumination");
		svgf_modulate_pass.set_texture_uniform(GL_TEXTURE_2D, init_normal_depth, "gNormalAndLinearZ");
		svgf_modulate_pass.draw();
		//-----------------------------------
		output_pass.reset_texture_slot();
		if (debug_window.accumulate_color) {
			output_pass.set_texture_uniform(GL_TEXTURE_2D, curColor, "texPass0");
		}
		else if (debug_window.final_pic) {
			output_pass.set_texture_uniform(GL_TEXTURE_2D, modulate_color, "texPass0");
		}
		else if(debug_window.svgf_reprojected_pic)
		{
			output_pass.set_texture_uniform(GL_TEXTURE_2D, curIllumination, "texPass0");
		}
		else if (debug_window.svgf_variance_pic) {
			output_pass.set_texture_uniform(GL_TEXTURE_2D, variance_compute_illumination, "texPass0");
		}
		else if (debug_window.svg_atrous_pic) {
			output_pass.set_texture_uniform(GL_TEXTURE_2D, modulate_color, "texPass0");
		}
		else if (debug_window.path_tracing_pic_1spp) {
			output_pass.set_texture_uniform(GL_TEXTURE_2D, curColor, "texPass0");
		}
		
		output_pass.draw();
		//-------------------------------

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		glfwSwapBuffers(window);
		glfwPollEvents();
		cameraRotate = inverse(cameraRotate);
		pre_viewproj = projection * cameraRotate;

		camera.frameCounter++;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;

}

void cursor_position_callback(GLFWwindow* window, double x, double y) {
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse) {
		int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
		if (state == GLFW_PRESS) {
			camera.frameCounter = 0;

			// 调整旋转
			camera.rotatAngle += 150 * (x - lastX) / camera.width;
			camera.upAngle += 150 * (y - lastY) / camera.height;
			camera.upAngle = glm::min(camera.upAngle, 89.0f);
			camera.upAngle = glm::max(camera.upAngle, -89.0f);
			lastX = x, lastY = y;
			camera.dirty = true;
		}
	}
}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			lastX = xpos, lastY = ypos;
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse) {
		camera.frameCounter = 0;
		int dir = yoffset > 0 ? 1 : -1;
		camera.r_dis += -dir * 0.5;
		camera.dirty = true;
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
	camera.dirty = true;
}

