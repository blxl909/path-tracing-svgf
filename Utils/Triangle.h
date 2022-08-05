#ifndef TRIANGLE_H
#define TRIANGLE_H

struct Triangle {
	glm::vec3 p1, p2, p3;    // 顶点坐标
	glm::vec3 n1, n2, n3;    // 顶点法线
	glm::vec2 uv1, uv2, uv3; // uv坐标
	Material material;  // 材质
	int objID;
};

struct Triangle_encoded {
	glm::vec3 p1, p2, p3;    // 顶点坐标
	glm::vec3 n1, n2, n3;    // 顶点法线
	glm::vec3 emissive;      // 自发光参数
	glm::vec3 baseColor;     // 颜色
	glm::vec3 param1;        // (subsurface, metallic, specular)
	glm::vec3 param2;        // (specularTint, roughness, anisotropic)
	glm::vec3 param3;        // (sheen, sheenTint, clearcoat)
	glm::vec3 param4;        // (clearcoatGloss, IOR, transmission)
	glm::vec3 uvPacked1;
	glm::vec3 uvPacked2;
	glm::vec3 objIndex;     //obj index存储在第一个位置，其他两个没用
};

#endif
