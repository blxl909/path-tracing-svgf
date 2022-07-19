#ifndef TRIANGLE_H
#define TRIANGLE_H


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Utils/Material.h"

struct Triangle {
	glm::vec3 p1, p2, p3;    // ��������
	glm::vec3 n1, n2, n3;    // ���㷨��
	glm::vec2 uv1, uv2, uv3; // uv����
	Material material;  // ����
	int objID;
};

#endif
