#ifndef TRIANGLE_H
#define TRIANGLE_H

struct Triangle {
	glm::vec3 p1, p2, p3;    // ��������
	glm::vec3 n1, n2, n3;    // ���㷨��
	glm::vec2 uv1, uv2, uv3; // uv����
	Material material;  // ����
	int objID;
};

struct Triangle_encoded {
	glm::vec3 p1, p2, p3;    // ��������
	glm::vec3 n1, n2, n3;    // ���㷨��
	glm::vec3 emissive;      // �Է������
	glm::vec3 baseColor;     // ��ɫ
	glm::vec3 param1;        // (subsurface, metallic, specular)
	glm::vec3 param2;        // (specularTint, roughness, anisotropic)
	glm::vec3 param3;        // (sheen, sheenTint, clearcoat)
	glm::vec3 param4;        // (clearcoatGloss, IOR, transmission)
	glm::vec3 uvPacked1;
	glm::vec3 uvPacked2;
	glm::vec3 objIndex;     //obj index�洢�ڵ�һ��λ�ã���������û��
};

#endif
