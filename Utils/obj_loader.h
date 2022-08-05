#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

// 读取 obj  can't read more than one file 
void readObj(std::string filepath, std::vector<float>& verticesout, std::vector<Triangle>& triangles, Material material, glm::mat4 trans, bool smoothNormal, int objIndex) {

	std::vector<glm::vec3> vertices;
	std::vector<GLint> indices;
	std::vector<GLint> tex_indices;
	std::vector<glm::vec2> texcoords;

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
			glm::vec3 tmp = glm::vec3(x, y, z);
			vertices.push_back(tmp);
			maxx = glm::max(maxx, x); maxy = glm::max(maxx, y); maxz = glm::max(maxx, z);
			minx = glm::min(minx, x); miny = glm::min(minx, y); minz = glm::min(minx, z);
		}

		if (type == "vt") {
			sin >> uvx >> uvy;//currently only support 2d texture coordinate
			texcoords.push_back(glm::vec2(uvx, uvy));
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
		glm::vec4 vv = glm::vec4(v.x, v.y, v.z, 1);
		vv = trans * vv;
		v = glm::vec3(vv.x, vv.y, vv.z);
	}

	// 生成法线
	std::vector<glm::vec3> normals(vertices.size(), glm::vec3(0, 0, 0));
	for (int i = 0; i < indices.size(); i += 3) {
		glm::vec3 p1 = vertices[indices[i]];
		glm::vec3 p2 = vertices[indices[i + 1]];
		glm::vec3 p3 = vertices[indices[i + 2]];
		glm::vec3 n = normalize(cross(p2 - p1, p3 - p1));
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
			glm::vec3 n = normalize(cross(t.p2 - t.p1, t.p3 - t.p1));
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

// 模型变换矩阵
glm::mat4 getTransformMatrix(glm::vec3 rotateCtrl, glm::vec3 translateCtrl, glm::vec3 scaleCtrl) {
	glm::mat4 unit(    // 单位矩阵
		glm::vec4(1, 0, 0, 0),
		glm::vec4(0, 1, 0, 0),
		glm::vec4(0, 0, 1, 0),
		glm::vec4(0, 0, 0, 1)
	);
	glm::mat4 scale = glm::scale(unit, scaleCtrl);
	glm::mat4 translate = glm::translate(unit, translateCtrl);
	glm::mat4 rotate = unit;
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.x), glm::vec3(1, 0, 0));
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.y), glm::vec3(0, 1, 0));
	rotate = glm::rotate(rotate, glm::radians(rotateCtrl.z), glm::vec3(0, 0, 1));

	glm::mat4 model = translate * rotate * scale;
	return model;
}

#endif
