#include "GraphicsTut6.h"
#include "GL_Manager.h"
#include "Application.h"
#include "Camera.h"

#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"
#include "Inputs.h"

GraphicsTut6::GraphicsTut6()
{
}


GraphicsTut6::~GraphicsTut6()
{
}


bool GraphicsTut6::update()
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
void GraphicsTut6::draw()
{
	OGL_Manager->UseShader(PointLight);
	OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("LightColour", vec3(1.0f, 1.0f, 1.0f));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("Brightness", 4.0f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->DrawFBX(BunnyModel, vec3(0, 0, 0));


	OGL_Manager->UseShader(Point_Textured);
	OGL_Manager->PassInUniform("LightPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("LightColour", vec3(1, 0.5f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("Brightness", 4.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	//OGL_Manager->SetTexture(SpearTex, 0, "diffuse");
	OGL_Manager->DrawFBX(SpearModel, vec3(-5, 0, -5));
	OGL_Manager->DrawFBX(SpearModel, vec3(5, 0, -5));
	OGL_Manager->DrawFBX(SpearModel, vec3(5, 0, 5));
	OGL_Manager->DrawFBX(SpearModel, vec3(-5, 0, 5));


	//PlaneDrawCall
	OGL_Manager->SetTexture(BoxTex, 0, "diffuse");
	OGL_Manager->SetTransform(glm::translate(vec3(0)));

	glBindVertexArray(m_VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);


	//Application::draw();
}
bool GraphicsTut6::startup()
{
	if (Application::startup())
	{
		//Models
		BunnyModel = OGL_Manager->AddFBXModel("./FBX/Bunny.fbx");
		SpearModel = OGL_Manager->AddFBXModel("./FBX/soulspear/soulspear.fbx");

		//Shaders
		PointLight = OGL_Manager->AddShaders("./Shaders/VS_PointLight.vert", "./Shaders/FS_PointLight.frag");
		DirectionalLight = OGL_Manager->AddShaders("./Shaders/VS_DirectionalLight.vert", "./Shaders/FS_DirectionalLight.frag");
		Textured = OGL_Manager->AddShaders("./Shaders/VS_Textured.vert", "./Shaders/FS_Textured.frag");
		Point_Textured = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured.vert", "./Shaders/FS_PointLight_Textured.frag");
		Point_Textured_bump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		BoxTex = OGL_Manager->AddTexture("./data/textures/crate.png");

		//CreatePlaneOGL_Buffers();
		CreatePlaneOGL_Buffers_wNormals();

		return true;
	}
	else
	{
		return false;
	}

}

void GraphicsTut6::CreatePlaneOGL_Buffers()
{
	float vertexData[] = {
		-5, 0, 5, 1, 0, 1,
		5, 0, 5, 1, 1, 1,
		5, 0, -5, 1, 1, 0,
		-5, 0, -5, 1, 0, 0,
	};

	unsigned int indexData[] = {
		0, 1, 2,
		0, 2, 3,
	};

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, vertexData, GL_STATIC_DRAW); //6 floats long, 4 floats deep, vertexData[];

	glGenBuffers(1, &m_IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, indexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, ((char*)0) + 16);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}

void GraphicsTut6::CreatePlaneOGL_Buffers_wNormals()
{
	float vertexData[] = {
		-5, 0, 5, 1, 0, 1, 0, 1, 0, 1,
		5, 0, 5, 1, 0, 1, 0, 1, 1, 1,
		5, 0, -5, 1, 0, 1, 0, 1, 1, 0,
		-5, 0, -5, 1, 0, 1, 0, 1, 0, 0,
	};

	unsigned int indexData[] = {
		0, 1, 2,
		0, 2, 3,
	};

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 10 * 4, vertexData, GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &m_IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, indexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 10, 0); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, the 0 seams to indicate how many bytes into the proccess to start
	glEnableVertexAttribArray(1); // One seems to indicate the location = 1 in the vertex shader , again that same number for is the first call in the following line
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 10, ((char*)0) + 16); //the 10 here seems to indicate the length of the strip of data in VertexBuffer , the 16 seams to indicate how many bytes into the proccess to start
	glEnableVertexAttribArray(2); // two seems to indicate the location = 2 in the vertex shader , again that same number for is the first call in the following line
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 10, ((char*)0) + 32); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, , the 32 seams to indicate how many bytes into the proccess to start

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}