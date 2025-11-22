#include "GraphicsTut9.h"

#include <loadgen/gl_core_4_4.h>
#include "glm\glm.hpp"
#include "glm\ext.hpp"

//MyIncludes
#include "GL_Manager.h"
#include "Camera.h"
#include "Vertex.h"


GraphicsTut9::GraphicsTut9()
{
}


GraphicsTut9::~GraphicsTut9()
{
}


bool GraphicsTut9::update()
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
void GraphicsTut9::draw()
{
	vec3 LightPosition = appBasics->AppCamera->GetDirVector() * 10 + appBasics->AppCamera->GetPos();
	OGL_Manager->UseShader(PointTexturedBump);
	OGL_Manager->PassInUniform("LightPos", LightPosition);
	OGL_Manager->PassInUniform("LightColour", vec3(1, 1.0f, 1));
	OGL_Manager->PassInUniform("CameraPos", appBasics->AppCamera->GetPos());
	OGL_Manager->PassInUniform("SpecPower", 1.5f);
	OGL_Manager->PassInUniform("Brightness", 30.5f);
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());

	OGL_Manager->PassInUniform("SpecIntensity", 0.2f);
	OGL_Manager->SetTexture(GrassDiffuse, 0, "diffuse");
	OGL_Manager->SetTexture(GrassNormal, 1, "normal");


	OGL_Manager->SetTransform(glm::translate(glm::vec3(0, 0, 0)));
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, Indexs, GL_UNSIGNED_INT, nullptr);

	OGL_Manager->DrawCustomGeometry(TestGeometry, vec3(0));

	Application::draw();
}


bool GraphicsTut9::startup()
{
	if (Application::startup())
	{
		//Shaders
		PointTexturedBump = OGL_Manager->AddShaders("./Shaders/VS_PointLight_Textured_Bump.vert", "./Shaders/FS_PointLight_Textured_Bump.frag");

		//Textures
		GrassDiffuse = OGL_Manager->AddTexture("./data/textures/Stone.jpg");
		GrassNormal = OGL_Manager->AddTexture("./data/textures/StoneN.jpg");

		GenTerrain();


		vector<VertexComplex> Points;
		vector<unsigned int> Indexs;

		Points.push_back({ 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 });
		Points.push_back({ 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1 });
		Points.push_back({ 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0 });
		Points.push_back({ 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1 });

		Indexs.push_back(0);
		Indexs.push_back(1);
		Indexs.push_back(2);
		Indexs.push_back(3);
		Indexs.push_back(2);
		Indexs.push_back(1);

		TestGeometry = OGL_Manager->AddCustomGeometry(Points, Indexs);

		return true;
	}
	else
	{
		return false;
	}
}

void GraphicsTut9::GenPerlin()
{
	PerlinData = new float[Elliments];
	
	for (int i = 0; i < XScale; ++i)
	{
		for (int j = 0; j < YScale; ++j)
		{
			float Amp = Height;
			float Persistence = 0.3f;
			PerlinData[j*XScale + i] = 0;

			for (int k = 0; k < Octaves; ++k)
			{
				float Freq = powf(4, (float)k)/30;
				float PerlinSample = glm::perlin(vec2((float)i, (float)j)*Scale*Freq)*0.5f + 0.5f;
				PerlinData[j*XScale + i] += PerlinSample*Amp;
				Amp *= Persistence;
			}

			//PerlinData[j*XScale + i] = rand() / 100;
			//PerlinData[j*XScale + i] = glm::perlin(vec2((float)i*Scale, (float)j*Scale)*VerticalScale)*0.5f + 0.5f;
		}
	}

}
void GraphicsTut9::GenTerrain()
{
	GenPerlin();
	VertexComplex* Array;
	unsigned int* indexData;
	Array = new VertexComplex[Elliments];
	indexData = new unsigned int[Indexs];

	for (int i = 0; i < XScale; ++i)
	{
		for (int j = 0; j < YScale; ++j)
		{
			Array[j*XScale+i].x = i*Scale;
			Array[j*XScale+i].z = j*Scale;
			Array[j*XScale+i].y = PerlinData[j*XScale + i];
			Array[j*XScale+i].w = 1;

			Array[j*XScale+i].nx = 0;
			Array[j*XScale+i].ny = 1;
			Array[j*XScale+i].nz = 0;
			Array[j*XScale+i].nw = 0;

			Array[j*XScale+i].tx = 1;
			Array[j*XScale+i].ty = 0;
			Array[j*XScale+i].tz = 0;
			Array[j*XScale+i].tw = 0;

			Array[j*XScale + i].s = 0/*(float)i*5 / (float)XScale*/;
			Array[j*XScale + i].t = 0/*(float)j*5 / (float)YScale*/;
		}
	}
	for (int i = 0; i < XScale-1; ++i)
	{
		for (int j = 0; j < YScale-1; ++j)
		{
			indexData[(j*(XScale - 1) + i) * 6 + 0] = j*XScale + i;
			indexData[(j*(XScale - 1) + i) * 6 + 1] = j*XScale + i +1;
			indexData[(j*(XScale - 1) + i) * 6 + 2] = (j+1)*XScale + i;
			indexData[(j*(XScale - 1) + i) * 6 + 3] = (j+1)*XScale + i;
			indexData[(j*(XScale - 1) + i) * 6 + 4] = (j+1)*XScale + i+1;
			indexData[(j*(XScale - 1) + i) * 6 + 5] = j*XScale + i +1;
		}
	}
	
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex) * Elliments, Array, GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indexs, indexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

	glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 12);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 4);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 8);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}