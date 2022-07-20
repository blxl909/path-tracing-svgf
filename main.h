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
	vec3 p1, p2, p3;    // ��������
	vec3 n1, n2, n3;    // ���㷨��
	vec3 emissive;      // �Է������
	vec3 baseColor;     // ��ɫ
	vec3 param1;        // (subsurface, metallic, specular)
	vec3 param2;        // (specularTint, roughness, anisotropic)
	vec3 param3;        // (sheen, sheenTint, clearcoat)
	vec3 param4;        // (clearcoatGloss, IOR, transmission)
	vec3 uvPacked1;     
	vec3 uvPacked2;
	vec3 objIndex;     //obj index�洢�ڵ�һ��λ�ã���������û��
};

struct BVHNode_encoded {
	vec3 childs;        // (left, right, ����)
	vec3 leafInfo;      // (n, index, ����)
	vec3 AA, BB;
};

std::string readShaderFile(std::string filepath) {
	std::string res, line;
	std::ifstream fin(filepath);
	if (!fin.is_open())
	{
		std::cout << "�ļ� " << filepath << " ��ʧ��" << std::endl;
		exit(-1);
	}
	while (std::getline(fin, line))
	{
		res += line + '\n';
	}
	fin.close();
	return res;
}
//�������岢û����Ȳ���
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
	// ��ȡshaderԴ�ļ�
	std::string vSource = readShaderFile(vshader);
	std::string fSource = readShaderFile(fshader);
	const char* vpointer = vSource.c_str();
	const char* fpointer = fSource.c_str();

	// �ݴ�
	GLint success;
	GLchar infoLog[512];

	// ���������붥����ɫ��
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, (const GLchar**)(&vpointer), NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);   // ������
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "������ɫ���������\n" << infoLog << std::endl;
		exit(-1);
	}

	// �������ұ���Ƭ����ɫ��
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const GLchar**)(&fpointer), NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);   // ������
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "Ƭ����ɫ���������\n" << infoLog << std::endl;
		exit(-1);
	}

	// ����������ɫ����program����
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// ɾ����ɫ������
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// ----------------------------------------------------------------------------- //

// ģ�ͱ任����
mat4 getTransformMatrix(vec3 rotateCtrl, vec3 translateCtrl, vec3 scaleCtrl) {
	glm::mat4 unit(    // ��λ����
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

// ��ȡ obj  can't read more than one file //to do  yes you know what
void readObj(std::string filepath, std::vector<float>& verticesout, std::vector<Triangle>& triangles, Material material, mat4 trans, bool smoothNormal,int objIndex) {

	std::vector<vec3> vertices;
	std::vector<GLint> indices;
	std::vector<GLint> tex_indices;
	std::vector<vec2> texcoords;

	// ���ļ���
	std::ifstream fin(filepath);
	std::string line;
	if (!fin.is_open()) {
		std::cout << "�ļ� " << filepath << " ��ʧ��" << std::endl;
		exit(-1);
	}

	// ���� AABB �У���һ��ģ�ʹ�С
	float maxx = -11451419.19;
	float maxy = -11451419.19;
	float maxz = -11451419.19;
	float minx = 11451419.19;
	float miny = 11451419.19;
	float minz = 11451419.19;

	// ���ж�ȡ
	while (std::getline(fin, line)) {
		std::istringstream sin(line);   // ��һ�е�������Ϊ string stream �������Ҷ�ȡ
		std::string type;
		GLfloat x, y, z;
		GLfloat uvx, uvy;
		int v0, v1, v2;
		int vn0, vn1, vn2;
		int vt0, vt1, vt2;
		char slash;

		// ͳ��б����Ŀ���ò�ͬ��ʽ��ȡ
		int slashCnt = 0;
		for (int i = 0; i < line.length(); i++) {
			if (line[i] == '/') slashCnt++;
		}

		// ��ȡobj�ļ�
		sin >> type;
		if (type == "v") {
			sin >> x >> y >> z;
			vec3 tmp = vec3(x, y, z);
			vertices.push_back(tmp);
			maxx = glm::max(maxx, x); maxy = glm::max(maxx, y); maxz = glm::max(maxx, z);
			minx = glm::min(minx, x); miny = glm::min(minx, y); minz = glm::min(minx, z);
		}
		// index�� �������ҵ� ��Ҫ�޸�
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

	// ģ�ʹ�С��һ��
	float lenx = maxx - minx;
	float leny = maxy - miny;
	float lenz = maxz - minz;
	float maxaxis = glm::max(lenx, glm::max(leny, lenz));
	for (auto& v : vertices) {
		v.x /= maxaxis;
		v.y /= maxaxis;
		v.z /= maxaxis;
	}

	// ͨ�������������任
	for (auto& v : vertices) {
		vec4 vv = vec4(v.x, v.y, v.z, 1);
		vv = trans * vv;
		v = vec3(vv.x, vv.y, vv.z);
	}

	// ���ɷ���
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




	// ���� Triangle ��������
	int offset = triangles.size();  // ��������

	triangles.resize(offset + indices.size() / 3);
	for (int i = 0; i < indices.size(); i += 3) {
		Triangle& t = triangles[offset + i / 3];
		// ����������
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

		// ������
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






// ���� HDR ��ͼ��ػ�����Ϣ
float* calculateHdrCache(float* HDR, int width, int height) {

	float lumSum = 0.0;

	// ��ʼ�� h �� w �еĸ����ܶ� pdf �� ͳ��������
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

	// �����ܶȹ�һ��
	for (int i = 0; i < height; i++)
		for (int j = 0; j < width; j++)
			pdf[i][j] /= lumSum;

	// �ۼ�ÿһ�еõ� x �ı�Ե�����ܶ�
	std::vector<float> pdf_x_margin;
	pdf_x_margin.resize(width);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_x_margin[j] += pdf[i][j];

	// ���� x �ı�Ե�ֲ�����
	std::vector<float> cdf_x_margin = pdf_x_margin;
	for (int i = 1; i < width; i++)
		cdf_x_margin[i] += cdf_x_margin[i - 1];

	// ���� y �� X=x �µ����������ܶȺ���
	std::vector<std::vector<float>> pdf_y_condiciton = pdf;
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			pdf_y_condiciton[i][j] /= pdf_x_margin[j];

	// ���� y �� X=x �µ��������ʷֲ�����
	std::vector<std::vector<float>> cdf_y_condiciton = pdf_y_condiciton;
	for (int j = 0; j < width; j++)
		for (int i = 1; i < height; i++)
			cdf_y_condiciton[i][j] += cdf_y_condiciton[i - 1][j];

	// cdf_y_condiciton ת��Ϊ���д洢
	// cdf_y_condiciton[i] ��ʾ y �� X=i �µ��������ʷֲ�����
	std::vector<std::vector<float>> temp = cdf_y_condiciton;
	cdf_y_condiciton = std::vector<std::vector<float>>(width);
	for (auto& line : cdf_y_condiciton) line.resize(height);
	for (int j = 0; j < width; j++)
		for (int i = 0; i < height; i++)
			cdf_y_condiciton[j][i] = temp[i][j];

	// ��� xi_1, xi_2 Ԥ�������� xy
	// sample_x[i][j] ��ʾ xi_1=i/height, xi_2=j/width ʱ (x,y) �е� x
	// sample_y[i][j] ��ʾ xi_1=i/height, xi_2=j/width ʱ (x,y) �е� y
	// sample_p[i][j] ��ʾȡ (i, j) ��ʱ�ĸ����ܶ�
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

			// �� xi_1 �� cdf_x_margin �� lower bound �õ����� x
			int x = std::lower_bound(cdf_x_margin.begin(), cdf_x_margin.end(), xi_1) - cdf_x_margin.begin();
			// �� xi_2 �� X=x ������µõ����� y
			int y = std::lower_bound(cdf_y_condiciton[x].begin(), cdf_y_condiciton[x].end(), xi_2) - cdf_y_condiciton[x].begin();

			// �洢�������� xy �� xy λ�ö�Ӧ�ĸ����ܶ�
			sample_x[i][j] = float(x) / width;
			sample_y[i][j] = float(y) / height;
			sample_p[i][j] = pdf[i][j];
		}
	}

	// ���Ͻ��������
	// R,G ͨ���洢���� (x,y) �� B ͨ���洢 pdf(i, j)
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
class NormalRenderPass {//��դ��pass
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

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorAttachments[i], 0);// ����ɫ����󶨵� i ����ɫ����

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
		// ���� finalPass ������֡�������ɫ����
		if (!finalPass) {
			std::vector<GLuint> attachments;
			for (int i = 0; i < colorAttachments.size(); i++) {
				glBindTexture(GL_TEXTURE_2D, colorAttachments[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorAttachments[i], 0);// ����ɫ����󶨵� i ����ɫ����
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
		// ����һ֡��֡������ɫ����
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