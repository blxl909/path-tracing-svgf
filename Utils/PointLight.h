#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
