#include "main.h"


void cursor_position_callback(GLFWwindow* window, double x, double y);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);


GLuint init_normal_depth;
GLuint init_world_position;
GLuint init_velocity;

GLuint curColor;
GLuint curNormalDepth;
GLuint Albedo;

GLuint reprojectedColor;
GLuint reprojectedmomenthistory;

GLuint variance_filtered_color;

GLuint atrous_flitered_color0;//swap buffer

GLuint tmp_atrous_result;

GLuint next_frame_color_input;//same as lastColor 

GLuint taa_output;

GLuint last_acc_color;
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


unsigned int frameCounter = 0;
float upAngle = 10.0;
float rotatAngle = 0.0;
float r_dis = 2.0;

mat4 pre_viewproj = glm::perspective(glm::radians(90.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);

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

	std::vector<Triangle> triangles;

	int objIndex = 0;

	// when number is under 0.0, that means we use texture to define it 
	Material m;
	m.roughness = -1.0;
	m.specular = 0.0;//1.0
	m.metallic = -1.0;//1.0
	m.clearcoat = 0.0;//1.0
	m.clearcoatGloss = 0.0;
	m.baseColor = vec3(-1, -1, -1);
	std::vector<float> vertices;
	//std::vector<GLuint> indices;
	readObj("models/clock.obj", vertices, triangles, m, getTransformMatrix(vec3(0, -20, 0), vec3(0, 0, 0), vec3(1, 1, 1)), true, objIndex++);

	m.roughness = -1.0;
	m.metallic = -1.0;
	m.specular = 0.0;
	m.baseColor = vec3(-1, -1, -1);
	float len = 13000.0;
	//readObj("models/plant.obj", vertices, triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(-0.75, -0.5, 0), vec3(2, 2, 2)), false, objIndex++);

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

	// hdr 全景图
	HDRLoaderResult hdrRes;
	bool r = HDRLoader::load("./HDR/room.hdr", hdrRes);
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

	init_pass.colorAttachments.push_back(init_world_position);
	init_pass.colorAttachments.push_back(init_normal_depth);
	init_pass.colorAttachments.push_back(init_velocity);

	init_pass.bindData(vertices);

	glUseProgram(init_pass.program);
	glUniform1i(glGetUniformLocation(init_pass.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(init_pass.program, "screen_height"), SCR_HEIGHT);
	glUseProgram(0);

//--------------------
	pass_path_tracing.program = getShaderProgram("./shaders/path_tracing.frag", "./shaders/vshader.vsh");

	curColor = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);
	curNormalDepth = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);
	Albedo = getTextureRGB32F(pass_path_tracing.width, pass_path_tracing.height);
	pass_path_tracing.colorAttachments.push_back(curColor);
	pass_path_tracing.colorAttachments.push_back(curNormalDepth);
	pass_path_tracing.colorAttachments.push_back(Albedo);

	pass_path_tracing.bindData(false);

	glUseProgram(pass_path_tracing.program);
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "nTriangles"), triangles.size());
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "nNodes"), nodes.size());
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "width"), pass_path_tracing.width);
	glUniform1i(glGetUniformLocation(pass_path_tracing.program, "height"), pass_path_tracing.height);
	glUseProgram(0);

//---------------------------------------

	pass2.program = getShaderProgram("./shaders/svgf_reproject.frag", "./shaders/vshader.vsh");
	reprojectedColor = getTextureRGB32F(pass2.width, pass2.height);
	reprojectedmomenthistory = getTextureRGB32F(pass2.width, pass2.height);
	pass2.colorAttachments.push_back(reprojectedColor);
	pass2.colorAttachments.push_back(reprojectedmomenthistory);

	pass2.bindData(false);

	glUseProgram(pass2.program);
	glUniform1i(glGetUniformLocation(pass2.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(pass2.program, "screen_height"), SCR_HEIGHT);
	glUseProgram(0);
//----------------------------
	pass_moment_filter.program = getShaderProgram("./shaders/svgf_variance.frag", "./shaders/vshader.vsh");
	variance_filtered_color = getTextureRGB32F(pass_moment_filter.width, pass_moment_filter.height);
	pass_moment_filter.colorAttachments.push_back(variance_filtered_color);

	pass_moment_filter.bindData(false);

	glUseProgram(pass_moment_filter.program);
	glUniform1i(glGetUniformLocation(pass_moment_filter.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(pass_moment_filter.program, "screen_height"), SCR_HEIGHT);
	glUseProgram(0);

//----------------------------
	pass_Atrous_filter.program = getShaderProgram("./shaders/svgf_Atrous.frag", "./shaders/vshader.vsh");
	atrous_flitered_color0 = getTextureRGB32F(pass_Atrous_filter.width, pass_Atrous_filter.height);
	pass_Atrous_filter.colorAttachments.push_back(atrous_flitered_color0);
	pass_Atrous_filter.bindData(false);

	glUseProgram(pass_Atrous_filter.program);
	glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "screen_width"), SCR_WIDTH);
	glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "screen_height"), SCR_HEIGHT);


	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_z"), 1.0f);
	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_n"), 128.0f);
	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_l"), 4.0f);
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
//----------------------------
	pass_pre_info.program = getShaderProgram("./shaders/passlastframe.frag", "./shaders/vshader.vsh");
	lastColor = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastNormalDepth = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastMomentHistory = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	last_color_taa = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	last_acc_color = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);

	pass_pre_info.colorAttachments.push_back(lastColor);
	pass_pre_info.colorAttachments.push_back(lastNormalDepth);
	pass_pre_info.colorAttachments.push_back(lastMomentHistory);
	pass_pre_info.colorAttachments.push_back(last_color_taa);
	pass_pre_info.colorAttachments.push_back(last_acc_color);

	pass_pre_info.bindData(false);
//----------------------------
	bool path_tracing_pic_1spp = false;
	bool svgf_reprojected_pic = false;
	bool final_pic = false;
	bool accumulate_color = true;
	bool use_normal_texture = false;
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		float fps = 1.0 / deltaTime;
		std::cout << "\r";
		std::cout << std::fixed << std::setprecision(2) << "FPS : " << fps << "    迭代次数: " << frameCounter;

		vec3 eye = vec3(-sin(radians(rotatAngle)) * cos(radians(upAngle)), sin(radians(upAngle)), cos(radians(rotatAngle)) * cos(radians(upAngle)));
		eye.x *= r_dis; eye.y *= r_dis; eye.z *= r_dis;
		mat4 cameraRotate = lookAt(eye, vec3(0, 0, 0), vec3(0, 1, 0));  // 相机注视着原点



		glm::mat4 view = cameraRotate;
		glm::mat4 projection = glm::perspective(glm::radians(90.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 1000.0f);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		

		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("show pass image");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("pick a pass to show the output.");               // Display some text (you can use a format strings too)
			if (ImGui::Checkbox("path_tracing_pic_1spp", &path_tracing_pic_1spp)) {
				frameCounter = 0;
				path_tracing_pic_1spp = true;
				svgf_reprojected_pic = false;
				final_pic = false;
				accumulate_color = false;
			}
			if (ImGui::Checkbox("svgf_reprojected_pic", &svgf_reprojected_pic)) {
				frameCounter = 0;
				path_tracing_pic_1spp = false;
				svgf_reprojected_pic = true;
				final_pic = false;
				accumulate_color = false;
			}
			if (ImGui::Checkbox("final_pic", &final_pic)) {
				frameCounter = 0;
				path_tracing_pic_1spp = false;
				svgf_reprojected_pic = false;
				final_pic = true;
				accumulate_color = false;
			}
			if (ImGui::Checkbox("show_accumulate_color", &accumulate_color)) {
				frameCounter = 0;
				path_tracing_pic_1spp = false;
				svgf_reprojected_pic = false;
				final_pic = false;
				accumulate_color = true;
			}
			if (ImGui::Checkbox("use_normal_texture", &use_normal_texture)) {
				frameCounter = 0;
			}
				

			//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			//ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			//if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			//	counter++;
			//ImGui::SameLine();
			//ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		


		glUseProgram(init_pass.program);
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "view"), 1, GL_FALSE, value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "projection"), 1, GL_FALSE, value_ptr(projection));

		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "pre_viewproj"), 1, GL_FALSE, value_ptr(pre_viewproj));
		glUniform1ui(glGetUniformLocation(init_pass.program, "frameCounter"), frameCounter++);

		init_pass.draw();
//-------------------------------------------
		cameraRotate = inverse(cameraRotate);
		glUseProgram(pass_path_tracing.program);
		glUniform3fv(glGetUniformLocation(pass_path_tracing.program, "eye"), 1, value_ptr(eye));
		glUniformMatrix4fv(glGetUniformLocation(pass_path_tracing.program, "cameraRotate"), 1, GL_FALSE, value_ptr(cameraRotate));
		glUniform1ui(glGetUniformLocation(pass_path_tracing.program, "frameCounter"), frameCounter);  // 传计数器用作随机种子
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "hdrResolution"), hdrResolution);   // hdf 分辨率
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "use_normal_map"), use_normal_texture);
		//cur hard code it ,set tbo later
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "pointLightSize"), 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_BUFFER, trianglesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "triangles"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER, nodesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "nodes"), 1);

		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "accumulate"), accumulate_color);
		if (accumulate_color) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, last_acc_color);
			glUniform1i(glGetUniformLocation(pass_path_tracing.program, "lastFrame"), 2);
		}
		
		

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, hdrMap);
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "hdrMap"), 3);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, hdrCache);
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "hdrCache"), 4);

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D_ARRAY, materials_array);
		glUniform1i(glGetUniformLocation(pass_path_tracing.program, "material_array"), 5);

		pass_path_tracing.draw();
//-------------------------------
		glUseProgram(pass2.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, curColor);
		glUniform1i(glGetUniformLocation(pass2.program, "texPass0"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, init_normal_depth);
		glUniform1i(glGetUniformLocation(pass2.program, "texPass1"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, init_world_position);
		glUniform1i(glGetUniformLocation(pass2.program, "texPass2"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, lastNormalDepth);
		glUniform1i(glGetUniformLocation(pass2.program, "lastNormalDepth"), 3);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, lastColor);
		glUniform1i(glGetUniformLocation(pass2.program, "lastColor"), 4);

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, lastMomentHistory);
		glUniform1i(glGetUniformLocation(pass2.program, "lastMomentHistory"), 5);

		/*glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, init_velocity);
		glUniform1i(glGetUniformLocation(pass2.program, "velocity"), 6);*/

		//mat4 inv_viewproj=inverse(inverse(cameraRotate)*camera.perspective_mat);
		//glUniformMatrix4fv(glGetUniformLocation(pass2.program, "viewproj"), 1, GL_FALSE, value_ptr(cameraRotate));
		glUniformMatrix4fv(glGetUniformLocation(pass2.program, "pre_viewproj"), 1, GL_FALSE, value_ptr(pre_viewproj));


		pass2.draw();

//-------------------------------
 		glUseProgram(pass_moment_filter.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, reprojectedColor);
		glUniform1i(glGetUniformLocation(pass_moment_filter.program, "reprojected_color"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, reprojectedmomenthistory);
		glUniform1i(glGetUniformLocation(pass_moment_filter.program, "reprojected_moment_history"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, init_normal_depth);
		glUniform1i(glGetUniformLocation(pass_moment_filter.program, "normal_depth"), 2);

		glUniform1f(glGetUniformLocation(pass_moment_filter.program, "sigma_z"), 1.0f);
		glUniform1f(glGetUniformLocation(pass_moment_filter.program, "sigma_n"), 128.0f);
		glUniform1f(glGetUniformLocation(pass_moment_filter.program, "sigma_l"), 4.0f);

		pass_moment_filter.draw();
//-------------------------------
 		int num_atrous_iterations = 5;




		for (int i = 0; i < num_atrous_iterations; i++) {
			glUseProgram(pass_Atrous_filter.program);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, init_normal_depth);//curNormalDepth
			glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "normal_depth"), 0);

			int step_size = 1 << i;
			glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "step_size"), step_size);

			if (i == 0) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, variance_filtered_color);
				glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "variance_computed_color"), 1);
			}
			else {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, tmp_atrous_result);
				glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "variance_computed_color"), 1);
			}
			pass_Atrous_filter.draw();
			glUseProgram(0);

			if (i == 1) {
				glUseProgram(save_next_frame_pass.program);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, atrous_flitered_color0);
				glUniform1i(glGetUniformLocation(save_next_frame_pass.program, "in_texture"), 0);
				save_next_frame_pass.draw();
				glUseProgram(0);
			}

			glUseProgram(bilt_pass.program);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, atrous_flitered_color0);
			glUniform1i(glGetUniformLocation(bilt_pass.program, "in_texture"), 0);
			bilt_pass.draw();
			glUseProgram(0);

		}
//-------------------------------
		glUseProgram(pass_taa.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, atrous_flitered_color0);
		glUniform1i(glGetUniformLocation(pass_taa.program, "currentColor"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, last_color_taa);
		glUniform1i(glGetUniformLocation(pass_taa.program, "previousColor"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, init_velocity);
		glUniform1i(glGetUniformLocation(pass_taa.program, "velocityTexture"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, init_normal_depth);
		glUniform1i(glGetUniformLocation(pass_taa.program, "normal_depth"), 3);

		pass_taa.draw();
//-------------------------------
		glUseProgram(pass_pre_info.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, next_frame_color_input);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass0"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, init_normal_depth);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass1"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, reprojectedmomenthistory);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass2"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, taa_output);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "curTAAoutput"), 3);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, curColor);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "curAccColor"), 4);

		pass_pre_info.draw();

//-------------------------------

		glUseProgram(pass3.program);
		if (path_tracing_pic_1spp) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, curColor);
			glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);
		}
		else if (svgf_reprojected_pic) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, reprojectedColor);
			glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);
		}
		else if (final_pic) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, taa_output);
			glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);
		}
		else if(accumulate_color){
			glActiveTexture(GL_TEXTURE0);
			//glBindTexture(GL_TEXTURE_2D, last_acc_color);
			glBindTexture(GL_TEXTURE_2D, last_acc_color);//albedo
			glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);

			//glActiveTexture(GL_TEXTURE1);
			////glBindTexture(GL_TEXTURE_2D, last_acc_color);
			//glBindTexture(GL_TEXTURE_2D, taa_output);
			//glUniform1i(glGetUniformLocation(pass3.program, "texPass1"), 1);

			//glActiveTexture(GL_TEXTURE2);
			////glBindTexture(GL_TEXTURE_2D, last_acc_color);
			//glBindTexture(GL_TEXTURE_2D, curNormalDepth);
			//glUniform1i(glGetUniformLocation(pass3.program, "texPass2"),2);
		}
		else {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, taa_output);
			glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);
		}

		pass3.draw();
//--------------------------------
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		glfwSwapBuffers(window);
		glfwPollEvents();
		cameraRotate = inverse(cameraRotate);
		pre_viewproj = projection * cameraRotate;
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
			frameCounter = 0;

			// 调整旋转
			rotatAngle += 150 * (x - lastX) / SCR_WIDTH;
			upAngle += 150 * (y - lastY) / SCR_HEIGHT;
			upAngle = glm::min(upAngle, 89.0f);
			upAngle = glm::max(upAngle, -89.0f);
			lastX = x, lastY = y;
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
		frameCounter = 0;
		int dir = yoffset > 0 ? 1 : -1;
		r_dis += -dir * 0.5;
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}


