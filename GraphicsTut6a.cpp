#include "GraphicsTut6a.h"

#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"

//MyIncludes
#include "GL_Manager.h"
#include "Camera.h"
#include "Vertex.h"

GraphicsTut6a::GraphicsTut6a()
{
}


GraphicsTut6a::~GraphicsTut6a()
{
}


bool GraphicsTut6a::update()
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
void GraphicsTut6a::draw()
{

	vec3 LightPosition = appBasics->AppCamera->GetDirVector()*10 + appBasics->AppCamera->GetPos();

	//LightPosition.y += 15;
	Gizmos::addTransform(mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(LightPosition, 1)));
	//OGL_Manager->PassInUniform("SpecIntensity", 0.0f);
	//OGL_Manager->SetTransform(glm::translate(glm::vec3(0,0,10)));
	//OGL_Manager->SetTexture(RockDiffuse, 0, "diffuse");
	//OGL_Manager->SetTexture(RockNormal, 1, "normal");
	
	//glBindVertexArray(m_VAO);
	//glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	//
	//OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	//OGL_Manager->SetTexture(RockWallDiffuse, 0, "diffuse");
	//OGL_Manager->SetTexture(RockWallNormal, 1, "normal");


	//OGL_Manager->SetTransform(glm::translate(glm::vec3(0,0,0)));
	
	//glBindVertexArray(m_VAO);
	//glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

	OGL_Manager->UseShader(PointTexturedBump);
	OGL_Manager->PassInUniform("LightPos", LightPosition);
	OGL_Manager->PassInUniform("LightColour", vec3(1, 1.0f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.5f);
	OGL_Manager->PassInUniform("Brightness", 3.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());

	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTexture(GrassDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(GrassNormal, 1, "normal");
	for (int x = 4; x < 6; x++)
	{
		for (int y = 4; y < 6; y++)
		{
			OGL_Manager->SetTransform(glm::translate(glm::vec3(x * 10, 0, y * 10)));
			glBindVertexArray(m_VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		}
	}


	OGL_Manager->PassInUniform("SpecIntensity", 0.5f);
	OGL_Manager->SetTexture(TilesDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(TilesNormal, 1, "normal");
	for (int x = 3; x < 7; x++)
	{
		for (int y = 3; y < 7; y++)
		{
			OGL_Manager->SetTransform(glm::translate(glm::vec3(x * 10, 0, y * 10)));
			glBindVertexArray(m_VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		}
	}

	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTexture(GrassDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(GrassNormal, 1, "normal");
	for (int x = 0; x < 10; x++)
	{
		for (int y = 0; y < 10; y++)
		{
			OGL_Manager->SetTransform(glm::translate(glm::vec3(x * 10, 0, y * 10)));
			glBindVertexArray(m_VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		}
	}


	OGL_Manager->UseShader(SpecTexProgram);
	OGL_Manager->PassInUniform("LightPos", LightPosition);
	OGL_Manager->PassInUniform("LightColour", vec3(1, 1.0f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.0f);
	OGL_Manager->PassInUniform("Brightness", 3.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());

	OGL_Manager->SetTexture(SpearTex, 0, "diffuse");
	OGL_Manager->DrawFBX(Spear, vec3(50, 0, 50));
	
	
	
	
	

	//Application::draw();
}
bool GraphicsTut6a::startup()
{
	if (Application::startup())
	{

		//Model
		Spear = OGL_Manager->AddFBXModel("./FBX/soulspear/soulspear.fbx");

		//Shaders
		SpecTexProgram = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured.vert", "./Shaders/FS_PointLight_Textured.frag");
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		RockDiffuse = OGL_Manager->AddTexture("./data/textures/Stone.jpg");
		RockNormal = OGL_Manager->AddTexture("./data/textures/StoneN.jpg");
		RockWallDiffuse = OGL_Manager->AddTexture("./data/textures/RWD3.jpg");
		RockWallNormal = OGL_Manager->AddTexture("./data/textures/RWN.jpg");
		TilesDiffuse = OGL_Manager->AddTexture("./data/textures/Tiles.png");
		TilesNormal = OGL_Manager->AddTexture("./data/textures/TilesN.jpg");
		GrassDiffuse = OGL_Manager->AddTexture("./data/textures/Grass.jpg");
		GrassNormal = OGL_Manager->AddTexture("./data/textures/GrassN.jpg");
		SpearTex = OGL_Manager->AddTexture("./FBX/soulspear/soulspear_diffuse.tga");

		CreatePlaneOGL_Buffers();


		return true;
	}
	else
	{
		return false;
	}

}


void GraphicsTut6a::CreatePlaneOGL_Buffers()
{
	VertexComplex VertexData[] = {
		{-5,0,5,1,0,1,0,0,1,0,0,0,0,2},
		{5,0,5,1,0,1,0,0,1,0,0,0,2,2},
		{5,0,-5,1,0,1,0,0,1,0,0,0,2,0},
		{-5,0,-5,1,0,1,0,0,1,0,0,0,0,0},
	};

	unsigned int indexData[] = {
		0, 1, 2,
		0, 2, 3,
	};

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex)*4, VertexData, GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &m_IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, indexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), 0);  

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) +sizeof(float)*12);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float)*4);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float)*8);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}