#ifndef POINTLIGHT_H
#define POINTLIGHT_H

class PointLight {
public:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 radiance = glm::vec3(0);

	PointLight() {}

	PointLight(glm::vec3 _position, glm::vec3 _radiance) {
		position = _position;
		radiance = _radiance;
	}
};


#endif
