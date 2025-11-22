#include "GraphicsTut3.h"
#include "Vertex.h"
#include <loadgen/gl_core_4_4.h>
#include <iosfwd>
#include "glm\glm.hpp"
#include "glm\ext.hpp"

#include "Camera.h"
#include "Clock.h"

bool GraphicsTut3::startup()
{
	if (Application::startup())
	{
		VertexShadder = "#version 410\n \
						layout(location=0) in vec4 Position; \n \
						layout(location=1) in vec4 Colour; \n \
						out vec4 vColour; \n \
						uniform mat4 ProjectionView; \n \
						uniform float Time; \n \
						uniform float heightScale; \n \
						void main() \n { vColour = Colour; \n  vec4 P = Position; \n P.y += sin(Time + Position.x + Position.z)*heightScale; \n gl_Position = ProjectionView * P; }";

		FragmentShadder = "#version 410\n \
						in vec4 vColour; \n \
						out vec4 FragColour; \n \
						void main() \n { FragColour = vColour; }";

		int success = GL_FALSE;

		unsigned int VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
		unsigned int FragShadderID = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(VertexShaderID, 1, (const char **)&VertexShadder, 0);
		glCompileShader(VertexShaderID);
		glShaderSource(FragShadderID, 1, (const char **)&FragmentShadder, 0);
		glCompileShader(FragShadderID);

		m_ProgramID = glCreateProgram();
		glAttachShader(m_ProgramID, VertexShaderID);
		glAttachShader(m_ProgramID, FragShadderID);
		glLinkProgram(m_ProgramID);
		//glBindAttribLocation(VertexShaderID, 0, "Position");
		//glBindAttribLocation(VertexShaderID, 1, "Colour");
		glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &success);
		if (success == GL_FALSE)
		{
			int infoLogLength = 0;
			glGetShaderiv(m_ProgramID, GL_INFO_LOG_LENGTH, &infoLogLength);
			char* infoLog = new char[infoLogLength];

			glGetShaderInfoLog(m_ProgramID, infoLogLength, 0, infoLog);
			printf("Error: Failed to link shader program! \n");
			printf("%s\n", infoLog);
			delete[] infoLog;
		}

		glDeleteShader(FragShadderID);
		glDeleteShader(VertexShaderID);


		//BuffersCreation
		glGenBuffers(1, &m_VBO);
		glGenBuffers(1, &m_IBO);
		glGenVertexArrays(1, &m_VAO); //Generate VAO from VBO and IBO

		return true;
	}
	return false;
}

GraphicsTut3::GraphicsTut3()
{
	

}

bool GraphicsTut3::update()
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

void GraphicsTut3::draw()
{
	generateGrid(15, 15);
	//Application::draw();
}

void GraphicsTut3::generateGrid(unsigned int Rows, unsigned int Cols)
{
	Vertex* aoVerticies = new Vertex[Rows*Cols];

	//Vertex Definition - used in VBO
	for (unsigned int r = 0; r < Rows; ++r)
	{
		for (unsigned int c = 0; c < Cols; ++c)
		{
			//Positioning
			aoVerticies[r*Cols + c].Position = vec4((float)c, 0,(float)r, 1);

			//Colouring
			aoVerticies[r*Cols + c].Colour = glm::vec4((float)c / (float)Cols, (float)r / (float)Rows, 0.5, 1);

		}
	}

	//what order are Vertex's are used to make triangles from vertex Data - used in IBO
	unsigned int* auiIndices = new unsigned int[(Rows - 1)*(Cols - 1) * 6];
	unsigned int index = 0;

	for (unsigned int r = 0; r < (Rows - 1); ++r)
	{
		for (unsigned int c = 0; c < (Cols - 1); ++c)
		{
			//Triangle1
			auiIndices[index++] = r*Cols + c;
			auiIndices[index++] = (r + 1)*Cols + c;
			auiIndices[index++] = (r + 1)*Cols + (c + 1);

			//Triangle2
			auiIndices[index++] = r*Cols + c;
			auiIndices[index++] = (r + 1)*Cols + (c + 1);
			auiIndices[index++] = r*Cols + (c + 1);
		}
	}


	//BuffersCreation
	//glGenBuffers(1, &m_VBO);
	//glGenBuffers(1, &m_IBO);
	//glGenVertexArrays(1, &m_VAO); //Generate VAO from VBO and IBO

	//VAO
	glBindVertexArray(m_VAO); //Attach VAO for Inspection

	//VBO
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO); //Attach VBO for Inspection
	glBufferData(GL_ARRAY_BUFFER, (Rows*Cols)*sizeof(Vertex), aoVerticies, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(vec4)));

	//IBO

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO); //Attach IBO for Inspection
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (Rows - 1)*(Cols - 1) * 6 * sizeof(unsigned int), auiIndices, GL_STATIC_DRAW);


	//Close Off (Detachments)
	glBindVertexArray(0); //Detach VAO
	glBindBuffer(GL_ARRAY_BUFFER, 0); //Dettach VBO
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //Dettach IBO


	glUseProgram(m_ProgramID);

	//Uniforms
	unsigned int projectionViewUniform = glGetUniformLocation(m_ProgramID, "ProjectionView");
	glUniformMatrix4fv(projectionViewUniform, 1, false, glm::value_ptr(appBasics->AppCamera->GetProjectionView()));

	unsigned int TimeUniform = glGetUniformLocation(m_ProgramID, "Time");
	glUniform1f(TimeUniform, (float)appBasics->AppClock->GetProgramTime().Second);

	unsigned int ScaleUniform = glGetUniformLocation(m_ProgramID, "heightScale");
	glUniform1f(ScaleUniform, (float)0.4f);

	glBindVertexArray(m_VAO);
	unsigned int indexCount = (Rows - 1)*(Cols - 1) * 6;

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

	//BufferDeletion
	//glDeleteBuffers(1, &m_VBO);
	//glDeleteBuffers(1, &m_IBO);
	//glDeleteVertexArrays(1, &m_VAO);

	delete[] auiIndices;
	delete[] aoVerticies;


}


GraphicsTut3::~GraphicsTut3()
{
	//BufferDeletion
	glDeleteBuffers(1, &m_VBO);
	glDeleteBuffers(1, &m_IBO);
	glDeleteVertexArrays(1, &m_VAO);
}
