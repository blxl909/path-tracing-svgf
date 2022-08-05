#ifndef RENDER_PASS_H
#define RENDER_PASS_H

// ----------------------------------------------------------------------------- //
class Rasterize_RenderPass {//光栅化pass
public:
	GLuint FBO;
	GLuint vao, vbo;
	unsigned int rbo;
	GLuint program;

	std::vector<GLuint> colorAttachments;
	std::vector<float> vert;
	int triangle_size;
	int width = SCR_WIDTH;
	int height = SCR_HEIGHT;
	

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
		std::vector<glm::vec3> square = { glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(-1, 1, 0), glm::vec3(1, 1, 0), glm::vec3(-1, 1, 0), glm::vec3(1, -1, 0) };
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * square.size(), NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * square.size(), &square[0]);

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
	void reset_texture_slot() {
		texture_slot = 0;
	}
	void set_texture_uniform(GLenum target, GLuint texture, const GLchar* uniform_name) {
		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0 + texture_slot);
		glBindTexture(target, texture);
		glUniform1i(glGetUniformLocation(program, uniform_name), texture_slot++);
		//glUseProgram(0);
	}

	void set_uniform_mat4(const GLchar* uniform_name, glm::mat4 value) {
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



#endif