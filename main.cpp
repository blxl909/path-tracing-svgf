#include "main.h"


void cursor_position_callback(GLFWwindow* window, double x, double y);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);


GLuint init_normal_depth;
GLuint init_world_position;

GLuint reprojectedColor;
GLuint reprojectedmomenthistory;
//
GLuint curColor;
GLuint curNormalDepth;
GLuint curWorldPos;

GLuint variance_filtered_color;

GLuint atrous_flitered_color0;//swap buffer

GLuint tmp_atrous_result;

GLuint next_frame_color_input;//same as lastColor 

GLuint lastColor;
GLuint lastNormalDepth;
GLuint lastMomentHistory;

RenderPass pass1;
RenderPass pass2;
RenderPass pass_moment_filter;
RenderPass pass_Atrous_filter;//save info pass
RenderPass bilt_pass;
RenderPass save_next_frame_pass;
RenderPass pass_pre_info;
RenderPass pass3;
NormalRenderPass init_pass;


unsigned int frameCounter = 0;
float upAngle = 10.0;
float rotatAngle = 90.0;
float r_dis = 2.0;

mat4 pre_viewproj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

int main(int argc, char** argv)
{

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
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
	


	// tell GLFW to capture our mouse
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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

	Material m;
	m.roughness = 0.5;
	m.specular = 1.0;//1.0
	m.metallic = 1.0;//1.0
	m.clearcoat = 1.0;//1.0
	m.clearcoatGloss = 0.0;
	m.baseColor = vec3(1, 0.73, 0.25);
	std::vector<float> vertices;
	//std::vector<GLuint> indices;
	readObj("models/teapot.obj", vertices, triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0, -0.5, 0), vec3(0.75, 0.75, 0.75)), true);

	m.roughness = 0.01;
	m.metallic = 0.1;
	m.specular = 1.0;
	m.baseColor = vec3(1, 1, 1);
	float len = 13000.0;
	readObj("models/quad.obj", vertices, triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0, -0.5, 0), vec3(len, 0.01, len)), false);

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
	//buildBVH(triangles, nodes, 0, triangles.size() - 1, 8);
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
	bool r = HDRLoader::load("./HDR/chinese_garden_2k.hdr", hdrRes);
	hdrMap = getTextureRGB32F(hdrRes.width, hdrRes.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, hdrRes.width, hdrRes.height, 0, GL_RGB, GL_FLOAT, hdrRes.cols);

	// hdr 重要性采样 cache
	std::cout << "计算 HDR 贴图重要性采样 Cache, 当前分辨率: " << hdrRes.width << " " << hdrRes.height << std::endl;
	float* cache = calculateHdrCache(hdrRes.cols, hdrRes.width, hdrRes.height);
	hdrCache = getTextureRGB32F(hdrRes.width, hdrRes.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, hdrRes.width, hdrRes.height, 0, GL_RGB, GL_FLOAT, cache);
	hdrResolution = hdrRes.width;


	pass3.program = getShaderProgram("./shaders/pass3.frag", "./shaders/vshader.vsh");
	pass3.bindData(true);
//-----------------------
	init_pass.program = getShaderProgram("./shaders/normalfshader.frag", "./shaders/normalvshader.vsh");

	init_normal_depth = getTextureRGB32F(init_pass.width, init_pass.height);
	init_world_position = getTextureRGB32F(init_pass.width, init_pass.height);

	init_pass.colorAttachments.push_back(init_world_position);
	init_pass.colorAttachments.push_back(init_normal_depth);
	

	init_pass.bindData(vertices);

//--------------------
	pass1.program = getShaderProgram("./shaders/fshader.frag", "./shaders/vshader.vsh");

	curColor = getTextureRGB32F(pass1.width, pass1.height);
	curNormalDepth = getTextureRGB32F(pass1.width, pass1.height);
	curWorldPos = getTextureRGB32F(pass1.width, pass1.height);
	pass1.colorAttachments.push_back(curColor);
	pass1.colorAttachments.push_back(curNormalDepth);
	pass1.colorAttachments.push_back(curWorldPos);

	pass1.bindData(false);

	glUseProgram(pass1.program);
	glUniform1i(glGetUniformLocation(pass1.program, "nTriangles"), triangles.size());
	glUniform1i(glGetUniformLocation(pass1.program, "nNodes"), nodes.size());
	glUniform1i(glGetUniformLocation(pass1.program, "width"), pass1.width);
	glUniform1i(glGetUniformLocation(pass1.program, "height"), pass1.height);
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
	pass_pre_info.program = getShaderProgram("./shaders/passlastframe.frag", "./shaders/vshader.vsh");
	lastColor = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastNormalDepth = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastMomentHistory = getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	pass_pre_info.colorAttachments.push_back(lastColor);
	pass_pre_info.colorAttachments.push_back(lastNormalDepth);
	pass_pre_info.colorAttachments.push_back(lastMomentHistory);

	pass_pre_info.bindData(false);
//----------------------------

	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		vec3 eye = vec3(-sin(radians(rotatAngle)) * cos(radians(upAngle)), sin(radians(upAngle)), cos(radians(rotatAngle)) * cos(radians(upAngle)));
		eye.x *= r_dis; eye.y *= r_dis; eye.z *= r_dis;
		mat4 cameraRotate = lookAt(eye, vec3(0, 0, 0), vec3(0, 1, 0));  // 相机注视着原点



		glm::mat4 view = cameraRotate;
		glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

		glUseProgram(init_pass.program);
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "view"), 1, GL_FALSE, value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "projection"), 1, GL_FALSE, value_ptr(projection));

		init_pass.draw();
//-------------------------------------------
		cameraRotate = inverse(cameraRotate);
		glUseProgram(pass1.program);
		glUniform3fv(glGetUniformLocation(pass1.program, "eye"), 1, value_ptr(eye));
		glUniformMatrix4fv(glGetUniformLocation(pass1.program, "cameraRotate"), 1, GL_FALSE, value_ptr(cameraRotate));
		glUniform1ui(glGetUniformLocation(pass1.program, "frameCounter"), frameCounter++);  // 传计数器用作随机种子
		glUniform1i(glGetUniformLocation(pass1.program, "hdrResolution"), hdrResolution);   // hdf 分辨率

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_BUFFER, trianglesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass1.program, "triangles"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER, nodesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass1.program, "nodes"), 1);

		/*glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, lastColor);
		glUniform1i(glGetUniformLocation(pass1.program, "lastFrame"), 2);*/

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, hdrMap);
		glUniform1i(glGetUniformLocation(pass1.program, "hdrMap"), 3);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, hdrCache);
		glUniform1i(glGetUniformLocation(pass1.program, "hdrCache"), 4);

		pass1.draw();
//-------------------------------
		glUseProgram(pass2.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, curColor);
		glUniform1i(glGetUniformLocation(pass2.program, "texPass0"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, curNormalDepth);
		glUniform1i(glGetUniformLocation(pass2.program, "texPass1"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, curWorldPos);
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
		glBindTexture(GL_TEXTURE_2D, curNormalDepth);
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
			glBindTexture(GL_TEXTURE_2D, curNormalDepth);
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
		glUseProgram(pass_pre_info.program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, next_frame_color_input);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass0"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, curNormalDepth);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass1"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, reprojectedmomenthistory);
		glUniform1i(glGetUniformLocation(pass_pre_info.program, "texPass2"), 2);

		pass_pre_info.draw();

//-------------------------------

		glUseProgram(pass3.program);


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, atrous_flitered_color0);
		glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);

		pass3.draw();
//--------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		cameraRotate = inverse(cameraRotate);
		pre_viewproj = projection * cameraRotate;
	}

	glfwTerminate();
	return 0;

}

void cursor_position_callback(GLFWwindow* window, double x, double y) {
	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (state == GLFW_PRESS) {
		frameCounter = 0;

		// 调整旋转
		rotatAngle += 150 * (x - lastX) / 512;
		upAngle += 150 * (y - lastY) / 512;
		upAngle = glm::min(upAngle, 89.0f);
		upAngle = glm::max(upAngle, -89.0f);
		lastX = x, lastY = y;
	}
	
}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		lastX = xpos, lastY = ypos;
	}

}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	frameCounter = 0;
	int dir = yoffset > 0 ? 1 : -1;
	r_dis += -dir * 0.5;
}