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
#include <algorithm>
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

using namespace glm;


unsigned int rbo;
GLuint trianglesTextureBuffer;
GLuint nodesTextureBuffer;
GLuint pointLightBuffer;

GLuint hdrMap;
GLuint hdrCache;
int hdrResolution;

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;


float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

Camera camera;

// ----------------------------------------------------------------------------- //

struct Triangle_encoded {
	vec3 p1, p2, p3;    // 顶点坐标
	vec3 n1, n2, n3;    // 顶点法线
	vec3 emissive;      // 自发光参数
	vec3 baseColor;     // 颜色
	vec3 param1;        // (subsurface, metallic, specular)
	vec3 param2;        // (specularTint, roughness, anisotropic)
	vec3 param3;        // (sheen, sheenTint, clearcoat)
	vec3 param4;        // (clearcoatGloss, IOR, transmission)
	vec3 uvPacked1;     
	vec3 uvPacked2;
	vec3 objIndex;     //obj index存储在第一个位置，其他两个没用
};

struct BVHNode_encoded {
	vec3 childs;        // (left, right, 保留)
	vec3 leafInfo;      // (n, index, 保留)
	vec3 AA, BB;
};

std::string readShaderFile(std::string filepath) {
	std::string res, line;
	std::ifstream fin(filepath);
	if (!fin.is_open())
	{
		std::cout << "文件 " << filepath << " 打开失败" << std::endl;
		exit(-1);
	}
	while (std::getline(fin, line))
	{
		res += line + '\n';
	}
	fin.close();
	return res;
}
//离屏缓冲并没有深度测试
GLuint getTextureRGB32F(int width, int height) {
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}

GLuint getShaderProgram(std::string fshader, std::string vshader) {
	// 读取shader源文件
	std::string vSource = readShaderFile(vshader);
	std::string fSource = readShaderFile(fshader);
	const char* vpointer = vSource.c_str();
	const char* fpointer = fSource.c_str();

	// 容错
	GLint success;
	GLchar infoLog[512];

	// 创建并编译顶点着色器
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, (const GLchar**)(&vpointer), NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);   // 错误检测
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "顶点着色器编译错误\n" << infoLog << std::endl;
		exit(-1);
	}

	// 创建并且编译片段着色器
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const GLchar**)(&fpointer), NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);   // 错误检测
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "片段着色器编译错误\n" << infoLog << std::endl;
		exit(-1);
	}

	// 链接两个着色器到program对象
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// 删除着色器对象
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// ----------------------------------------------------------------------------- //

// 模型变换矩阵
mat4 getTransformMatrix(vec3 rotateCtrl, vec3 translateCtrl, vec3 scaleCtrl) {
	glm::mat4 unit(    // 单位矩阵
		glm::vec4(1, 0, 0, 0),
		glm::vec4(0, 1, 0, 0),
		glm::vec4(0, 0, 1, 0),
		glm::vec4(0, 0, 0, 1)
	);
	mat4 scale = glm::scale(unit, scaleCtrl);
	mat4 translate = glm::translate(unit, translateCtrl);
	mat4 rotate = unit;
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.x), glm::vec3(1, 0, 0));
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.y), glm::vec3(0, 1, 0));
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.z), glm::vec3(0, 0, 1));

	//mat4 perspective = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);

	mat4 model = translate * rotate * scale;
	return model;
}

// 读取 obj  can't read more than one file //to do  yes you know what
void readObj(std::string filepath, std::vector<float>& verticesout, std::vector<Triangle>& triangles, Material material, mat4 trans, bool smoothNormal,int objIndex) {

	std::vector<vec3> vertices;
	std::vector<GLint> indices;
	std::vector<GLint> tex_indices;
	std::vector<vec2> texcoords;

	// 打开文件流
	std::ifstream fin(filepath);
	std::string line;
	if (!fin.is_open()) {
		std::cout << "文件 " << filepath << " 打开失败" << std::endl;
		exit(-1);
	}

	// 计算 AABB 盒，归一化模型大小
	float maxx = -11451419.19;
	float maxy = -11451419.19;
	float maxz = -11451419.19;
	float minx = 11451419.19;
	float miny = 11451419.19;
	float minz = 11451419.19;

	// 按行读取
	while (std::getline(fin, line)) {
		std::istringstream sin(line);   // 以一行的数据作为 string stream 解析并且读取
		std::string type;
		GLfloat x, y, z;
		GLfloat uvx, uvy;
		int v0, v1, v2;
		int vn0, vn1, vn2;
		int vt0, vt1, vt2;
		char slash;

		// 统计斜杆数目，用不同格式读取
		int slashCnt = 0;
		for (int i = 0; i < line.length(); i++) {
			if (line[i] == '/') slashCnt++;
		}

		// 读取obj文件
		sin >> type;
		if (type == "v") {
			sin >> x >> y >> z;
			vec3 tmp = vec3(x, y, z);
			vertices.push_back(tmp);
			maxx = glm::max(maxx, x); maxy = glm::max(maxx, y); maxz = glm::max(maxx, z);
			minx = glm::min(minx, x); miny = glm::min(minx, y); minz = glm::min(minx, z);
		}
		// index和 坐标是乱的 需要修改
		if (type == "vt") {
			sin >> uvx >> uvy;//currently only support 2d texture coordinate
			texcoords.push_back(vec2(uvx, uvy));
		}
		if (type == "f") {
			if (slashCnt == 6) {
				sin >> v0 >> slash >> vt0 >> slash >> vn0;
				sin >> v1 >> slash >> vt1 >> slash >> vn1;
				sin >> v2 >> slash >> vt2 >> slash >> vn2;
			}
			else if (slashCnt == 3) {
				sin >> v0 >> slash >> vt0;
				sin >> v1 >> slash >> vt1;
				sin >> v2 >> slash >> vt2;
			}
			else {
				sin >> v0 >> v1 >> v2;
			}
			indices.push_back(v0 - 1);
			indices.push_back(v1 - 1);
			indices.push_back(v2 - 1);
			tex_indices.push_back(vt0 - 1);
			tex_indices.push_back(vt1 - 1);
			tex_indices.push_back(vt2 - 1);
		}
	}

	// 模型大小归一化
	float lenx = maxx - minx;
	float leny = maxy - miny;
	float lenz = maxz - minz;
	float maxaxis = glm::max(lenx, glm::max(leny, lenz));
	for (auto& v : vertices) {
		v.x /= maxaxis;
		v.y /= maxaxis;
		v.z /= maxaxis;
	}

	// 通过矩阵进行坐标变换
	for (auto& v : vertices) {
		vec4 vv = vec4(v.x, v.y, v.z, 1);
		vv = trans * vv;
		v = vec3(vv.x, vv.y, vv.z);
	}

	// 生成法线
	std::vector<vec3> normals(vertices.size(), vec3(0, 0, 0));
	for (int i = 0; i < indices.size(); i += 3) {
		vec3 p1 = vertices[indices[i]];
		vec3 p2 = vertices[indices[i + 1]];
		vec3 p3 = vertices[indices[i + 2]];
		vec3 n = normalize(cross(p2 - p1, p3 - p1));
		normals[indices[i]] += n;
		normals[indices[i + 1]] += n;
		normals[indices[i + 2]] += n;
	}




	// 构建 Triangle 对象数组
	int offset = triangles.size();  // 增量更新

	triangles.resize(offset + indices.size() / 3);
	for (int i = 0; i < indices.size(); i += 3) {
		Triangle& t = triangles[offset + i / 3];
		// 传顶点属性
		t.p1 = vertices[indices[i]];
		t.p2 = vertices[indices[i + 1]];
		t.p3 = vertices[indices[i + 2]];
		t.uv1 = texcoords[tex_indices[i]];
		t.uv2 = texcoords[tex_indices[i + 1]];
		t.uv3 = texcoords[tex_indices[i + 2]];
		t.objID = objIndex;
		if (!smoothNormal) {
			vec3 n = normalize(cross(t.p2 - t.p1, t.p3 - t.p1));
			t.n1 = n; t.n2 = n; t.n3 = n;
		}
		else {
			t.n1 = normalize(normals[indices[i]]);
			t.n2 = normalize(normals[indices[i + 1]]);
			t.n3 = normalize(normals[indices[i + 2]]);
		}

		// 传材质
		t.material = material;


		verticesout.push_back(t.p1.x);
		verticesout.push_back(t.p1.y);
		verticesout.push_back(t.p1.z);
		verticesout.push_back(t.n1.x);
		verticesout.push_back(t.n1.y);
		verticesout.push_back(t.n1.z);
		verticesout.push_back(t.p2.x);
		verticesout.push_back(t.p2.y);
		verticesout.push_back(t.p2.z);
		verticesout.push_back(t.n2.x);
		verticesout.push_back(t.n2.y);
		verticesout.push_back(t.n2.z);
		verticesout.push_back(t.p3.x);
		verticesout.push_back(t.p3.y);
		verticesout.push_back(t.p3.z);
		verticesout.push_back(t.n3.x);
		verticesout.push_back(t.n3.y);
		verticesout.push_back(t.n3.z);
	}

}






// 计算 HDR 贴图相关缓存信息
float* calculateHdrCache(float* HDR, int width, int height) {

	float lumSum = 0.0;

	// 初始化 h 行 w 列的概率密度 pdf 并 统计总亮度
	std::vector<std::vector<float>> pdf(height);
	for (auto& line : pdf) line.resize(width);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			float R = HDR[3 * (i * width + j)];
			float G = HDR[3 * (i * width + j) + 1];
			float B = HDR[3 * (i * width + j) + 2];
			float lum = 0.2 * R + 0.7 * G + 0.1 * B;
			pdf[i][j] = lum;
			lumSum += lum;
		}
	}

	// 概率密度归一化
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			pdf[i][j] /= lumSum;

	// 累加每一列得到 x 的边缘概率密度
	std::vector<float> pdf_x_margin;
	pdf_x_margin.resize(width);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_x_margin[j] += pdf[i][j];

	// 计算 x 的边缘分布函数
	std::vector<float> cdf_x_margin = pdf_x_margin;
	for (int i = 1; i < width; i++)
		cdf_x_margin[i] += cdf_x_margin[i - 1];

	// 计算 y 在 X=x 下的条件概率密度函数
	std::vector<std::vector<float>> pdf_y_condiciton = pdf;
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_y_condiciton[i][j] /= pdf_x_margin[j];

	// 计算 y 在 X=x 下的条件概率分布函数
	std::vector<std::vector<float>> cdf_y_condiciton = pdf_y_condiciton;
	for (int j = 0; j < width; j++)
		for (int i = 1; i < height; i++)
			cdf_y_condiciton[i][j] += cdf_y_condiciton[i - 1][j];

	// cdf_y_condiciton 转置为按列存储
	// cdf_y_condiciton[i] 表示 y 在 X=i 下的条件概率分布函数
	std::vector<std::vector<float>> temp = cdf_y_condiciton;
	cdf_y_condiciton = std::vector<std::vector<float>>(width);
	for (auto& line : cdf_y_condiciton) line.resize(height);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			cdf_y_condiciton[j][i] = temp[i][j];

	// 穷举 xi_1, xi_2 预计算样本 xy
	// sample_x[i][j] 表示 xi_1=i/height, xi_2=j/width 时 (x,y) 中的 x
	// sample_y[i][j] 表示 xi_1=i/height, xi_2=j/width 时 (x,y) 中的 y
	// sample_p[i][j] 表示取 (i, j) 点时的概率密度
	std::vector<std::vector<float>> sample_x(height);
	for (auto& line : sample_x) line.resize(width);
	std::vector<std::vector<float>> sample_y(height);
	for (auto& line : sample_y) line.resize(width);
	std::vector<std::vector<float>> sample_p(height);
	for (auto& line : sample_p) line.resize(width);
	for (int j = 0; j < width; j++) {
		for (int i = 0; i < height; i++) {
			float xi_1 = float(i) / height;
			float xi_2 = float(j) / width;

			// 用 xi_1 在 cdf_x_margin 中 lower bound 得到样本 x
			int x = std::lower_bound(cdf_x_margin.begin(), cdf_x_margin.end(), xi_1) - cdf_x_margin.begin();
			// 用 xi_2 在 X=x 的情况下得到样本 y
			int y = std::lower_bound(cdf_y_condiciton[x].begin(), cdf_y_condiciton[x].end(), xi_2) - cdf_y_condiciton[x].begin();

			// 存储纹理坐标 xy 和 xy 位置对应的概率密度
			sample_x[i][j] = float(x) / width;
			sample_y[i][j] = float(y) / height;
			sample_p[i][j] = pdf[i][j];
		}
	}

	// 整合结果到纹理
	// R,G 通道存储样本 (x,y) 而 B 通道存储 pdf(i, j)
	float* cache = new float[width * height * 3];
	//for (int i = 0; i < width * height * 3; i++) cache[i] = 0.0;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			cache[3 * (i * width + j)] = sample_x[i][j];        // R
			cache[3 * (i * width + j) + 1] = sample_y[i][j];    // G
			cache[3 * (i * width + j) + 2] = sample_p[i][j];    // B
		}
	}

	return cache;
}


// ----------------------------------------------------------------------------- //
class NormalRenderPass {//光栅化pass
public:
	GLuint FBO;
	GLuint vao, vbo;
	std::vector<GLuint> colorAttachments;
	int triangle_size;
	GLuint program;
	int width = SCR_WIDTH;
	int height = SCR_HEIGHT;
	std::vector<float> vert;

	void bindData(std::vector<float> vertices) {

		triangle_size = vertices.size() / 6;
		vert = vertices;

		glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);



		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vert[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (GLvoid*)(3 * sizeof(float)));



		std::vector<GLuint> attachments;
		for (int i = 0; i < colorAttachments.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, colorAttachments[i]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorAttachments[i], 0);// 将颜色纹理绑定到 i 号颜色附件

			attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
		}
		glDrawBuffers(attachments.size(), &attachments[0]);

		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height); // use a single renderbuffer object for both a depth AND stencil buffer.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);


		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void draw() {
		glUseProgram(program);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glBindVertexArray(vao);


		glViewport(0, 0, width, height);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//glDrawElements(GL_TRIANGLES, indice_size, GL_UNSIGNED_INT, 0);
		glDrawArrays(GL_TRIANGLES, 0, triangle_size * 3);
		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);
	}
};

class RenderPass {
public:
	GLuint FBO = 0;
	GLuint vao, vbo;
	std::vector<GLuint> colorAttachments;
	GLuint program;
	int width = SCR_WIDTH;
	int height = SCR_HEIGHT;
	GLint texture_slot = 0;

	void bindData(bool finalPass = false) {
		if (!finalPass) glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);


		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		std::vector<vec3> square = { vec3(-1, -1, 0), vec3(1, -1, 0), vec3(-1, 1, 0), vec3(1, 1, 0), vec3(-1, 1, 0), vec3(1, -1, 0) };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * square.size(), NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * square.size(), &square[0]);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glEnableVertexAttribArray(0);   // layout (location = 0) 
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		// 不是 finalPass 则生成帧缓冲的颜色附件
		if (!finalPass) {
			std::vector<GLuint> attachments;
			for (int i = 0; i < colorAttachments.size(); i++) {
				glBindTexture(GL_TEXTURE_2D, colorAttachments[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorAttachments[i], 0);// 将颜色纹理绑定到 i 号颜色附件
				attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
			}
			glDrawBuffers(attachments.size(), &attachments[0]);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	void draw(std::vector<GLuint> texPassArray = {}) {
		glUseProgram(program);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glBindVertexArray(vao);
		// 传上一帧的帧缓冲颜色附件
		for (int i = 0; i < texPassArray.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, texPassArray[i]);
			std::string uName = "texPass" + std::to_string(i);
			glUniform1i(glGetUniformLocation(program, uName.c_str()), i);
		}
		glViewport(0, 0, width, height);
		glDisable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);
	}
	void reset_texture_slot() {
		texture_slot = 0;
	}
	void set_texture_uniform(GLenum target,GLuint texture, const GLchar* uniform_name) {
		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0 + texture_slot);
		glBindTexture(target, texture);
		glUniform1i(glGetUniformLocation(program, uniform_name), texture_slot++);
		//glUseProgram(0);
	}

	void set_uniform_mat4(const GLchar* uniform_name,glm::mat4 value) {
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, uniform_name), 1, GL_FALSE, value_ptr(value));
	}

	void set_uniform_float(const GLchar* uniform_name, float value) {
		glUseProgram(program);
		glUniform1f(glGetUniformLocation(program, uniform_name), value);
	}

	void set_uniform_int(const GLchar* uniform_name, int value) {
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, uniform_name), value);
	}

	void set_uniform_uint(const GLchar* uniform_name, unsigned int value) {
		glUseProgram(program);
		glUniform1ui(glGetUniformLocation(program, uniform_name), value);
	}

	void set_uniform_bool(const GLchar* uniform_name, bool value) {
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, uniform_name), value);
	}

	void set_uniform_vec3(const GLchar* uniform_name, glm::vec3 value) {
		glUseProgram(program);
		glUniform3fv(glGetUniformLocation(program, uniform_name), 1, value_ptr(value));
	}

};

void load_texture_to_material_array(std::string img_path, GLint slot) {

	int width, height, nrChannels;

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(FileSystem::getPath(img_path).c_str(), &width, &height, &nrChannels, 0);
	
	if (data) {
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slot, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
	}
	else
	{
		std::cout << "Failed to load texture from " << img_path << std::endl;
	}

	stbi_image_free(data);
}