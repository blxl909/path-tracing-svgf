﻿#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>


#include <GL/glew.h>


#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>



#include "lib/hdrloader.h"

#define INF 114514.0

using namespace glm;

// ----------------------------------------------------------------------------- //

class Camera {
public:
	int width = 512;
	int height = 512;
	float near_plane = 0.00f;
	float far_plane = 100000.0f;
	float fovy = glm::radians(90.0f);
	mat4 perspective_mat = glm::perspective(fovy, float(width / height), near_plane, far_plane);
};



// 物体表面材质定义
struct Material {
	vec3 emissive = vec3(0, 0, 0);  // 作为光源时的发光颜色
	vec3 baseColor = vec3(1, 1, 1);
	float subsurface = 0.0;
	float metallic = 0.0;
	float specular = 0.5;
	float specularTint = 0.0;
	float roughness = 0.5;
	float anisotropic = 0.0;
	float sheen = 0.0;
	float sheenTint = 0.5;
	float clearcoat = 0.0;
	float clearcoatGloss = 1.0;
	float IOR = 1.0;
	float transmission = 0.0;
};

// 三角形定义
struct Triangle {
	vec3 p1, p2, p3;    // 顶点坐标
	vec3 n1, n2, n3;    // 顶点法线
	Material material;  // 材质
};

// BVH 树节点
struct BVHNode {
	int left, right;    // 左右子树索引
	int n, index;       // 叶子节点信息               
	vec3 AA, BB;        // 碰撞盒
};

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
};

struct BVHNode_encoded {
	vec3 childs;        // (left, right, 保留)
	vec3 leafInfo;      // (n, index, 保留)
	vec3 AA, BB;
};

unsigned int rbo;

// ----------------------------------------------------------------------------- //
class NormalRenderPass {//光栅化pass
public:
	GLuint FBO ;
	GLuint vao, vbo;
	std::vector<GLuint> colorAttachments;
	int triangle_size;
	GLuint program;
	int width = 512;
	int height = 512;
	std::vector<float> vert;
	
	void bindData(std::vector<float> vertices) {
		
		triangle_size = vertices.size()/6;
		vert = vertices;
		
		glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);

		

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), &vert[0], GL_STATIC_DRAW);
		
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
		glDrawArrays(GL_TRIANGLES, 0, triangle_size*3);
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
	int width = 512;
	int height = 512;
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
};

// ----------------------------------------------------------------------------- //

GLuint trianglesTextureBuffer;
GLuint nodesTextureBuffer;

GLuint hdrMap;
GLuint hdrCache;
int hdrResolution;
//lastcolor 取代lastframe
//----------------
GLuint curColor;
GLuint curNormalDepth;
GLuint curWorldPos;

GLuint reprojectedColor;
GLuint reprojectedmomenthistory;

GLuint variance_filtered_color;

GLuint atrous_flitered_color0;//swap buffer



GLuint lastColor;
GLuint lastNormalDepth;
GLuint lastMomentHistory;


GLuint tmp_atrous_result;
GLuint next_frame_color_input;//same as lastColor 

GLuint init_normal_depth;
GLuint init_clip_position;
//------------------

RenderPass pass1;//raytracingpass
RenderPass pass2;//reprojectionpass
RenderPass pass_moment_filter;//culculate moment in space and filter picture
RenderPass pass_Atrous_filter;//save info pass
RenderPass pass_pre_info;//save info pass
RenderPass bilt_pass;
RenderPass save_next_frame_pass;
RenderPass pass3;//postprocessingpass


NormalRenderPass init_pass;

// 相机参数
float upAngle = 0.0;
float rotatAngle = 0.0;
float r = 4.0;

// ----------------------------------------------------------------------------- //

// 按照三角形中心排序 -- 比较函数
bool cmpx(const Triangle& t1, const Triangle& t2) {
	vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / vec3(3, 3, 3);
	vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / vec3(3, 3, 3);
	return center1.x < center2.x;
}
bool cmpy(const Triangle& t1, const Triangle& t2) {
	vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / vec3(3, 3, 3);
	vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / vec3(3, 3, 3);
	return center1.y < center2.y;
}
bool cmpz(const Triangle& t1, const Triangle& t2) {
	vec3 center1 = (t1.p1 + t1.p2 + t1.p3) / vec3(3, 3, 3);
	vec3 center2 = (t2.p1 + t2.p2 + t2.p3) / vec3(3, 3, 3);
	return center1.z < center2.z;
}

// ----------------------------------------------------------------------------- //

// 读取文件并且返回一个长字符串表示文件内容
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

GLuint getTextureDepth(int width, int height) {
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}


// 获取着色器对象
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
void readObj(std::string filepath,std::vector<float>& verticesout,  std::vector<Triangle>& triangles, Material material, mat4 trans, bool smoothNormal) {

	std::vector<vec3> vertices;
	std::vector<GLint> indices;

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
			vertices.push_back(vec3(x, y, z));
			maxx = glm::max(maxx, x); maxy = glm::max(maxx, y); maxz = glm::max(maxx, z);
			minx = glm::min(minx, x); miny = glm::min(minx, y); minz = glm::min(minx, z);
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
	
	/*for (int i = 0; i < vertices.size(); i++) {
		verticesout.push_back(vertices[i].x);
		verticesout.push_back(vertices[i].y);
		verticesout.push_back(vertices[i].z);
		normals[i] = normalize(normals[i]);
		verticesout.push_back(normals[i].x);
		verticesout.push_back(normals[i].y);
		verticesout.push_back(normals[i].z);
	}
	for (int i = 0; i < indices.size(); i++) {
		indicesout.push_back(indices[i]);
	}
	std::cout << "triangles size    " << triangles.size() << std::endl;*/
}

// 构建 BVH
int buildBVH(std::vector<Triangle>& triangles, std::vector<BVHNode>& nodes, int l, int r, int n) {
	if (l > r) return 0;

	// 注：
	// 此处不可通过指针，引用等方式操作，必须用 nodes[id] 来操作
	// 因为 std::vector<> 扩容时会拷贝到更大的内存，那么地址就改变了
	// 而指针，引用均指向原来的内存，所以会发生错误
	nodes.push_back(BVHNode());
	int id = nodes.size() - 1;   // 注意： 先保存索引
	nodes[id].left = nodes[id].right = nodes[id].n = nodes[id].index = 0;
	nodes[id].AA = vec3(1145141919, 1145141919, 1145141919);
	nodes[id].BB = vec3(-1145141919, -1145141919, -1145141919);

	// 计算 AABB
	for (int i = l; i <= r; i++) {
		// 最小点 AA
		float minx = glm::min(triangles[i].p1.x, glm::min(triangles[i].p2.x, triangles[i].p3.x));
		float miny = glm::min(triangles[i].p1.y, glm::min(triangles[i].p2.y, triangles[i].p3.y));
		float minz = glm::min(triangles[i].p1.z, glm::min(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].AA.x = glm::min(nodes[id].AA.x, minx);
		nodes[id].AA.y = glm::min(nodes[id].AA.y, miny);
		nodes[id].AA.z = glm::min(nodes[id].AA.z, minz);
		// 最大点 BB
		float maxx = glm::max(triangles[i].p1.x, glm::max(triangles[i].p2.x, triangles[i].p3.x));
		float maxy = glm::max(triangles[i].p1.y, glm::max(triangles[i].p2.y, triangles[i].p3.y));
		float maxz = glm::max(triangles[i].p1.z, glm::max(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].BB.x = glm::max(nodes[id].BB.x, maxx);
		nodes[id].BB.y = glm::max(nodes[id].BB.y, maxy);
		nodes[id].BB.z = glm::max(nodes[id].BB.z, maxz);
	}

	// 不多于 n 个三角形 返回叶子节点
	if ((r - l + 1) <= n) {
		nodes[id].n = r - l + 1;
		nodes[id].index = l;
		return id;
	}

	// 否则递归建树
	float lenx = nodes[id].BB.x - nodes[id].AA.x;
	float leny = nodes[id].BB.y - nodes[id].AA.y;
	float lenz = nodes[id].BB.z - nodes[id].AA.z;
	// 按 x 划分
	if (lenx >= leny && lenx >= lenz)
		std::sort(triangles.begin() + l, triangles.begin() + r + 1, cmpx);
	// 按 y 划分
	if (leny >= lenx && leny >= lenz)
		std::sort(triangles.begin() + l, triangles.begin() + r + 1, cmpy);
	// 按 z 划分
	if (lenz >= lenx && lenz >= leny)
		std::sort(triangles.begin() + l, triangles.begin() + r + 1, cmpz);
	// 递归
	int mid = (l + r) / 2;
	int left = buildBVH(triangles, nodes, l, mid, n);
	int right = buildBVH(triangles, nodes, mid + 1, r, n);

	nodes[id].left = left;
	nodes[id].right = right;

	return id;
}

// SAH 优化构建 BVH
int buildBVHwithSAH(std::vector<Triangle>& triangles, std::vector<BVHNode>& nodes, int l, int r, int n) {
	if (l > r) return 0;

	nodes.push_back(BVHNode());
	int id = nodes.size() - 1;
	nodes[id].left = nodes[id].right = nodes[id].n = nodes[id].index = 0;
	nodes[id].AA = vec3(1145141919, 1145141919, 1145141919);
	nodes[id].BB = vec3(-1145141919, -1145141919, -1145141919);

	// 计算 AABB
	for (int i = l; i <= r; i++) {
		// 最小点 AA
		float minx = glm::min(triangles[i].p1.x, glm::min(triangles[i].p2.x, triangles[i].p3.x));
		float miny = glm::min(triangles[i].p1.y, glm::min(triangles[i].p2.y, triangles[i].p3.y));
		float minz = glm::min(triangles[i].p1.z, glm::min(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].AA.x = glm::min(nodes[id].AA.x, minx);
		nodes[id].AA.y = glm::min(nodes[id].AA.y, miny);
		nodes[id].AA.z = glm::min(nodes[id].AA.z, minz);
		// 最大点 BB
		float maxx = glm::max(triangles[i].p1.x, glm::max(triangles[i].p2.x, triangles[i].p3.x));
		float maxy = glm::max(triangles[i].p1.y, glm::max(triangles[i].p2.y, triangles[i].p3.y));
		float maxz = glm::max(triangles[i].p1.z, glm::max(triangles[i].p2.z, triangles[i].p3.z));
		nodes[id].BB.x = glm::max(nodes[id].BB.x, maxx);
		nodes[id].BB.y = glm::max(nodes[id].BB.y, maxy);
		nodes[id].BB.z = glm::max(nodes[id].BB.z, maxz);
	}

	// 不多于 n 个三角形 返回叶子节点
	if ((r - l + 1) <= n) {
		nodes[id].n = r - l + 1;
		nodes[id].index = l;
		return id;
	}

	// 否则递归建树
	float Cost = INF;
	int Axis = 0;
	int Split = (l + r) / 2;
	for (int axis = 0; axis < 3; axis++) {
		// 分别按 x，y，z 轴排序
		if (axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpx);
		if (axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpy);
		if (axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpz);

		// leftMax[i]: [l, i] 中最大的 xyz 值
		// leftMin[i]: [l, i] 中最小的 xyz 值
		std::vector<vec3> leftMax(r - l + 1, vec3(-INF, -INF, -INF));
		std::vector<vec3> leftMin(r - l + 1, vec3(INF, INF, INF));
		// 计算前缀 注意 i-l 以对齐到下标 0
		for (int i = l; i <= r; i++) {
			Triangle& t = triangles[i];
			int bias = (i == l) ? 0 : 1;  // 第一个元素特殊处理

			leftMax[i - l].x = glm::max(leftMax[i - l - bias].x, glm::max(t.p1.x, glm::max(t.p2.x, t.p3.x)));
			leftMax[i - l].y = glm::max(leftMax[i - l - bias].y, glm::max(t.p1.y, glm::max(t.p2.y, t.p3.y)));
			leftMax[i - l].z = glm::max(leftMax[i - l - bias].z, glm::max(t.p1.z, glm::max(t.p2.z, t.p3.z)));

			leftMin[i - l].x = glm::min(leftMin[i - l - bias].x, glm::min(t.p1.x, glm::min(t.p2.x, t.p3.x)));
			leftMin[i - l].y = glm::min(leftMin[i - l - bias].y, glm::min(t.p1.y, glm::min(t.p2.y, t.p3.y)));
			leftMin[i - l].z = glm::min(leftMin[i - l - bias].z, glm::min(t.p1.z, glm::min(t.p2.z, t.p3.z)));
		}

		// rightMax[i]: [i, r] 中最大的 xyz 值
		// rightMin[i]: [i, r] 中最小的 xyz 值
		std::vector<vec3> rightMax(r - l + 1, vec3(-INF, -INF, -INF));
		std::vector<vec3> rightMin(r - l + 1, vec3(INF, INF, INF));
		// 计算后缀 注意 i-l 以对齐到下标 0
		for (int i = r; i >= l; i--) {
			Triangle& t = triangles[i];
			int bias = (i == r) ? 0 : 1;  // 第一个元素特殊处理

			rightMax[i - l].x = glm::max(rightMax[i - l + bias].x, glm::max(t.p1.x, glm::max(t.p2.x, t.p3.x)));
			rightMax[i - l].y = glm::max(rightMax[i - l + bias].y, glm::max(t.p1.y, glm::max(t.p2.y, t.p3.y)));
			rightMax[i - l].z = glm::max(rightMax[i - l + bias].z, glm::max(t.p1.z, glm::max(t.p2.z, t.p3.z)));

			rightMin[i - l].x = glm::min(rightMin[i - l + bias].x, glm::min(t.p1.x, glm::min(t.p2.x, t.p3.x)));
			rightMin[i - l].y = glm::min(rightMin[i - l + bias].y, glm::min(t.p1.y, glm::min(t.p2.y, t.p3.y)));
			rightMin[i - l].z = glm::min(rightMin[i - l + bias].z, glm::min(t.p1.z, glm::min(t.p2.z, t.p3.z)));
		}

		// 遍历寻找分割
		float cost = INF;
		int split = l;
		for (int i = l; i <= r - 1; i++) {
			float lenx, leny, lenz;
			// 左侧 [l, i]
			vec3 leftAA = leftMin[i - l];
			vec3 leftBB = leftMax[i - l];
			lenx = leftBB.x - leftAA.x;
			leny = leftBB.y - leftAA.y;
			lenz = leftBB.z - leftAA.z;
			float leftS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
			float leftCost = leftS * (i - l + 1);

			// 右侧 [i+1, r]
			vec3 rightAA = rightMin[i + 1 - l];
			vec3 rightBB = rightMax[i + 1 - l];
			lenx = rightBB.x - rightAA.x;
			leny = rightBB.y - rightAA.y;
			lenz = rightBB.z - rightAA.z;
			float rightS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
			float rightCost = rightS * (r - i);

			// 记录每个分割的最小答案
			float totalCost = leftCost + rightCost;
			if (totalCost < cost) {
				cost = totalCost;
				split = i;
			}
		}
		// 记录每个轴的最佳答案
		if (cost < Cost) {
			Cost = cost;
			Axis = axis;
			Split = split;
		}
	}

	// 按最佳轴分割
	if (Axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpx);
	if (Axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpy);
	if (Axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, cmpz);

	// 递归
	int left = buildBVHwithSAH(triangles, nodes, l, Split, n);
	int right = buildBVHwithSAH(triangles, nodes, Split + 1, r, n);

	nodes[id].left = left;
	nodes[id].right = right;

	return id;
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

// 绘制
clock_t t1, t2;
double dt, fps;
unsigned int frameCounter = 0;
Camera camera = Camera();
mat4 pre_viewproj = camera.perspective_mat;
void display() {

	//if (frameCounter == 5) system("pause");

	// 帧计时
	t2 = clock();
	dt = (double)(t2 - t1) / CLOCKS_PER_SEC;
	fps = 1.0 / dt;
	std::cout << "\r";
	std::cout << std::fixed << std::setprecision(2) << "FPS : " << fps << "    迭代次数: " << frameCounter;
	t1 = t2;

	// 相机参数
	//eye的初始位置是(0,0,1)
	vec3 eye = vec3(-sin(radians(rotatAngle)) * cos(radians(upAngle)), sin(radians(upAngle)), cos(radians(rotatAngle)) * cos(radians(upAngle)));
	eye.x *= r; eye.y *= r; eye.z *= r;
	mat4 cameraRotate = lookAt(eye, vec3(0, 0, 0), vec3(0, 1, 0));  // 相机注视着原点
	//---------------


	glUseProgram(init_pass.program);
	glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "view"), 1, GL_FALSE, value_ptr(cameraRotate));
	glUniformMatrix4fv(glGetUniformLocation(init_pass.program, "projection"), 1, GL_FALSE, value_ptr(camera.perspective_mat));
	glClearDepth(1.0);
	glDepthMask(GL_TRUE);                       // Allow depth writing
	glEnable(GL_DEPTH_TEST);                    // Enable Depth testing
	glDepthFunc(GL_LESS);
	
	init_pass.draw();
	//------------------
	//--------------------------------
	cameraRotate = inverse(cameraRotate);   // lookat 的逆矩阵将光线方向进行转换

	
	
	glDisable(GL_DEPTH_TEST);
	// 传 uniform 给 pass1
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

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, lastColor);
	glUniform1i(glGetUniformLocation(pass1.program, "lastFrame"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, hdrMap);
	glUniform1i(glGetUniformLocation(pass1.program, "hdrMap"), 3);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, hdrCache);
	glUniform1i(glGetUniformLocation(pass1.program, "hdrCache"), 4);

	// 绘制
	pass1.draw();

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
//-------------------------
	
	glUseProgram(pass3.program);
	
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, next_frame_color_input);
	glUniform1i(glGetUniformLocation(pass3.program, "texPass0"), 0);
	
	

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, variance_filtered_color);
	glUniform1i(glGetUniformLocation(pass3.program, "texPass1"), 1);

	pass3.draw();

	glutSwapBuffers();

	//--------------
	cameraRotate = inverse(cameraRotate);
	pre_viewproj = camera.perspective_mat* cameraRotate;
	//std::cout << pre_viewproj[3][0] << std::endl;
	//--------------
}

// 每一帧
void frameFunc() {
	glutPostRedisplay();
}

// 鼠标运动函数
double lastX = 0.0, lastY = 0.0;
void mouse(int x, int y) {
	frameCounter = 0;
	// 调整旋转
	rotatAngle += 150 * (x - lastX) / 512;
	upAngle += 150 * (y - lastY) / 512;
	upAngle = glm::min(upAngle, 89.0f);
	upAngle = glm::max(upAngle, -89.0f);
	lastX = x, lastY = y;
	glutPostRedisplay();    // 重绘
}

// 鼠标按下
void mouseDown(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		lastX = x, lastY = y;
	}
}

// 鼠标滚轮函数
void mouseWheel(int wheel, int direction, int x, int y) {
	frameCounter = 0;
	r += -direction * 0.5;
	glutPostRedisplay();    // 重绘
}


//mouse mouseWheel

int main(int argc, char** argv)
{

	glutInit(&argc, argv);              // glut初始化
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(512, 512);// 窗口大小
	glutInitWindowPosition(350, 50);
	glutCreateWindow("Path Tracing GPU"); // 创建OpenGL上下文
	glewInit();
	glEnable(GL_CULL_FACE);
	// ----------------------------------------------------------------------------- //

	// camera config
	rotatAngle = 90;
	upAngle = 10;
	r = 2.0;

	// scene config
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
	//readObj("models/sphere2.obj", triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0, -0.1, 0), vec3(0.75, 0.75, 0.75)), true);
	//readObj("models/AnyConv.com__dragon_vrip_res2.obj", triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0.1, -0.9, 0), vec3(1.75, 1.75, 1.75)), true);

	

	m.roughness = 0.01;
	m.metallic = 0.1;
	m.specular = 1.0;
	m.baseColor = vec3(1, 1, 1);
	float len = 13000.0;
	//readObj("models/quad.obj", vertices, triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0, -0.5, 0), vec3(len, 0.01, len)), false);

	
	

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

	// ----------------------------------------------------------------------------- //

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

	// ----------------------------------------------------------------------------- //

	// 管线配置

	pass1.program = getShaderProgram("./shaders/fshader.frag", "./shaders/vshader.vsh");
	//pass1.width = pass1.height = 256;


//-------------
	curColor=getTextureRGB32F(pass1.width, pass1.height);
	curNormalDepth=getTextureRGB32F(pass1.width, pass1.height);
	curWorldPos=getTextureRGB32F(pass1.width, pass1.height);
	pass1.colorAttachments.push_back(curColor);
	pass1.colorAttachments.push_back(curNormalDepth);
	pass1.colorAttachments.push_back(curWorldPos);	
	//display还未设置 
//-------------
	pass1.bindData(false);

	glUseProgram(pass1.program);
	glUniform1i(glGetUniformLocation(pass1.program, "nTriangles"), triangles.size());
	glUniform1i(glGetUniformLocation(pass1.program, "nNodes"), nodes.size());
	glUniform1i(glGetUniformLocation(pass1.program, "width"), pass1.width);
	glUniform1i(glGetUniformLocation(pass1.program, "height"), pass1.height);
	glUseProgram(0);

	pass2.program = getShaderProgram("./shaders/svgf_reproject.frag", "./shaders/vshader.vsh");
	reprojectedColor = getTextureRGB32F(pass2.width, pass2.height);
	reprojectedmomenthistory = getTextureRGB32F(pass2.width, pass2.height);
	pass2.colorAttachments.push_back(reprojectedColor);
	pass2.colorAttachments.push_back(reprojectedmomenthistory);
	
	pass2.bindData(false);

	glUseProgram(pass2.program);
	glUniform1i(glGetUniformLocation(pass2.program, "screen_width"), camera.width);  
	glUniform1i(glGetUniformLocation(pass2.program, "screen_height"), camera.height);  
	glUseProgram(0);

	pass_moment_filter.program = getShaderProgram("./shaders/svgf_variance.frag", "./shaders/vshader.vsh");
	variance_filtered_color = getTextureRGB32F(pass_moment_filter.width, pass_moment_filter.height);
	pass_moment_filter.colorAttachments.push_back(variance_filtered_color);

	pass_moment_filter.bindData(false);

	glUseProgram(pass_moment_filter.program);
	glUniform1i(glGetUniformLocation(pass_moment_filter.program, "screen_width"), camera.width);  
	glUniform1i(glGetUniformLocation(pass_moment_filter.program, "screen_height"), camera.height);  
	glUseProgram(0);

	pass_Atrous_filter.program = getShaderProgram("./shaders/svgf_Atrous.frag", "./shaders/vshader.vsh");
	atrous_flitered_color0 = getTextureRGB32F(pass_Atrous_filter.width, pass_Atrous_filter.height);
	pass_Atrous_filter.colorAttachments.push_back(atrous_flitered_color0);
	pass_Atrous_filter.bindData(false);

	glUseProgram(pass_Atrous_filter.program);
	glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "screen_width"), camera.width);
	glUniform1i(glGetUniformLocation(pass_Atrous_filter.program, "screen_height"), camera.height);


	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_z"), 1.0f);
	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_n"), 128.0f);
	glUniform1f(glGetUniformLocation(pass_Atrous_filter.program, "sigma_l"), 4.0f);
	glUseProgram(0);
	

	bilt_pass.program= getShaderProgram("./shaders/bilt.frag", "./shaders/vshader.vsh");
	tmp_atrous_result=getTextureRGB32F(bilt_pass.width, bilt_pass.height);
	bilt_pass.colorAttachments.push_back(tmp_atrous_result);
	bilt_pass.bindData(false);

	save_next_frame_pass.program = getShaderProgram("./shaders/bilt.frag", "./shaders/vshader.vsh");
	next_frame_color_input = getTextureRGB32F(save_next_frame_pass.width, save_next_frame_pass.height);
	save_next_frame_pass.colorAttachments.push_back(next_frame_color_input);
	save_next_frame_pass.bindData(false);
	//next_frame_color_input= getTextureRGB32F(bilt_pass.width, bilt_pass.height);
	

	pass_pre_info.program=getShaderProgram("./shaders/passlastframe.frag", "./shaders/vshader.vsh");
	lastColor=getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastNormalDepth=getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	lastMomentHistory=getTextureRGB32F(pass_pre_info.width, pass_pre_info.height);
	pass_pre_info.colorAttachments.push_back(lastColor);
	pass_pre_info.colorAttachments.push_back(lastNormalDepth);
	pass_pre_info.colorAttachments.push_back(lastMomentHistory);

	pass_pre_info.bindData(false);

	pass3.program = getShaderProgram("./shaders/pass3.frag", "./shaders/vshader.vsh");
	pass3.bindData(true);

	init_pass.program = getShaderProgram("./shaders/normalfshader.frag", "./shaders/normalvshader.vsh");
	
	init_normal_depth= getTextureRGB32F(init_pass.width, init_pass.height);
	init_clip_position = getTextureRGB32F(init_pass.width, init_pass.height);
	
	init_pass.colorAttachments.push_back(init_normal_depth);
	init_pass.colorAttachments.push_back(init_clip_position);
	
	init_pass.bindData(vertices);//ebo
	

	// ----------------------------------------------------------------------------- //

	std::cout << "开始..." << std::endl << std::endl;

	glEnable(GL_DEPTH_TEST);  // 开启深度测试
	glClearColor(0.5, 0.5, 0.5, 1.0);   // 背景颜色 -- 黑

	glutDisplayFunc(display);   // 设置显示回调函数
	glutIdleFunc(frameFunc);    // 闲置时刷新
	glutMotionFunc(mouse);      // 鼠标拖动
	glutMouseFunc(mouseDown);   // 鼠标左键按下
	glutMouseWheelFunc(mouseWheel); // 滚轮缩放
	glutMainLoop();

	return 0;

}


