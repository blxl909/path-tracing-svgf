#ifndef HELP_FUNC_H
#define HELP_FUNC_H

void load_texture_to_material_array(std::string img_path, GLint slot) {

	int width, height, nrChannels;

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(FileSystem::getPath(img_path).c_str(), &width, &height, &nrChannels, 0);

	if (data) {
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slot, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
	}
	else
	{
		std::cout << "Failed to load texture from " << img_path << std::endl;
	}

	stbi_image_free(data);
}

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


#endif
