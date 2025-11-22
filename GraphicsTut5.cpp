#include "GraphicsTut5.h"
#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"
#include "Camera.h"

#include "stb-master\stb_image.h"

GraphicsTut5::GraphicsTut5()
{
}


GraphicsTut5::~GraphicsTut5()
{
	cleanupOpenGLBuffers();

	glDeleteProgram(m_Program);
}

bool GraphicsTut5::update()
{
	if (Application::update())
	{
		return true;
	}
	else
	{
		return false;
	}
}
void GraphicsTut5::draw()
{
	glUseProgram(m_Program);

	//bind the camera
	int loc = glGetUniformLocation(m_Program, "ProjectionView");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &(appBasics->AppCamera->GetProjectionView()[0][0]));

	//set texture slot
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texture);

	//tell the shader where it is
	loc = glGetUniformLocation(m_Program, "diffuse");
	glUniform1i(loc, 0);

	//draw
	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);



	//Application::draw();
}
bool GraphicsTut5::startup()
{
	if (Application::startup())
	{
		//Texture
		int imageWidth = 0, imageHeight = 0, imageFormat = 0;
		unsigned char* data = stbi_load("./data/textures/crate.png",&imageWidth,&imageHeight,&imageFormat,STBI_default);
		
		glGenTextures(1, &m_texture);
		glBindTexture(GL_TEXTURE_2D, m_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageWidth, imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		stbi_image_free(data);


		vsSource = "#version 410\n	\
				   	layout(location = 0) in vec4 Position;\n\
					layout(location = 1) in vec2 TexCoord;\n\
					out vec2 vTexCoord;\n\
					uniform mat4 ProjectionView;\n\
					void main() { \
					vTexCoord = TexCoord; \n \
					gl_Position = ProjectionView*Position;	}";


		fsSource = "#version 410\n\
				   	in vec2	vTexCoord;\n\
					out vec4 FragColor;\n\
					uniform sampler2D diffuse; \n \
					void main() { \
					FragColor = texture(diffuse,vTexCoord); }";
		
		VertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(VertexShader, 1, (const char**)&vsSource, 0);
		glCompileShader(VertexShader);

		FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(FragmentShader, 1, (const char**)&fsSource, 0);
		glCompileShader(FragmentShader);

		m_Program = glCreateProgram();
		glAttachShader(m_Program, VertexShader);
		glAttachShader(m_Program, FragmentShader);
		glLinkProgram(m_Program);

		glDeleteShader(VertexShader);
		glDeleteShader(FragmentShader);

		createOpenGLBuffers();
		
		return true;
	}
	else
	{
		return false;
	}

}

void GraphicsTut5::createOpenGLBuffers()
{
	float vertexData[] = {
		-5, 0, 5, 1, 0, 2,
		5, 0, 5, 1, 2, 2,
		5, 0, -5, 1, 2, 0,
		-5, 0, -5, 1, 0, 0,
	};

	unsigned int indexData[] = {
		0, 1, 2,
		0, 2, 3,
	};

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, vertexData, GL_STATIC_DRAW);

	glGenBuffers(1, &m_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, indexData, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, ((char*)0) + 16);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}

void GraphicsTut5::cleanupOpenGLBuffers()
{
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteBuffers(1, &m_ibo);
}