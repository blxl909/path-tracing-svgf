#ifndef CAMERA_H
#define CAMERA_H


const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

class Camera {
public:
	//currently the width and height have no effect on render target size 
	int width = int(SCR_WIDTH);
	int height = int(SCR_HEIGHT);

	float near_plane = 0.01f;
	float far_plane = 1000.0f;
	float fov_y = 90.0f;

	float upAngle = 10.0;
	float rotatAngle = 0.0;
	float r_dis = 2.0;

	float speed = 0.2f;
	glm::vec3 cam_position;
	glm::vec3 look_at_point = glm::vec3(0);
	glm::vec3 up_vec = glm::vec3(0, 1, 0);
	glm::vec3 move_vec = glm::vec3(0);
	unsigned int frameCounter = 0;
	bool dirty = false;

	glm::mat4 cam_proj_mat;
	glm::mat4 cam_view_mat;

public:
	Camera() {
		cam_position = glm::vec3(-glm::sin(glm::radians(rotatAngle)) * glm::cos(glm::radians(upAngle)), glm::sin(glm::radians(upAngle)), glm::cos(glm::radians(rotatAngle)) * glm::cos(glm::radians(upAngle)));
		cam_position.x *= r_dis; cam_position.y *= r_dis; cam_position.z *= r_dis;
		cam_proj_mat = glm::perspective(glm::radians(90.0f), (float)width / (float)height, near_plane, far_plane);
		cam_view_mat = glm::lookAt(cam_position, move_vec, glm::vec3(0, 1, 0));
	}

	void processInput_key(GLFWwindow* window, float deltaTime) {
		float velocity = deltaTime * speed;
		glm::vec3 viewDir = glm::normalize(look_at_point - cam_position);
		glm::vec3 right = glm::normalize(glm::cross(viewDir, up_vec));
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			move_vec += speed * viewDir;
			dirty = true;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			move_vec -= speed * viewDir;
			dirty = true;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			move_vec -= speed * right;
			dirty = true;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			move_vec += speed * right;
			dirty = true;
		}
	}
	void update(GLFWwindow* window, float deltaTime) {
		processInput_key(window, deltaTime);
		if (dirty) {
			glm::vec3 eye = glm::vec3(-glm::sin(glm::radians(rotatAngle)) * glm::cos(glm::radians(upAngle)), glm::sin(glm::radians(upAngle)), glm::cos(glm::radians(rotatAngle)) * glm::cos(glm::radians(upAngle)));
			eye.x *= r_dis; eye.y *= r_dis; eye.z *= r_dis;
			eye += move_vec;
			cam_position = eye;
			look_at_point = move_vec;
			cam_view_mat = glm::lookAt(cam_position, look_at_point, glm::vec3(0, 1, 0));
			frameCounter = 0;
			dirty = false;
		}
	}
};

#endif