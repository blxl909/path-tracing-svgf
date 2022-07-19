#ifndef TRIANGLE_H
#define TRIANGLE_H


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Utils/Material.h"

struct Triangle {
	glm::vec3 p1, p2, p3;    // 顶点坐标
	glm::vec3 n1, n2, n3;    // 顶点法线
	glm::vec2 uv1, uv2, uv3; // uv坐标
	Material material;  // 材质
	int objID;
};

#endif
