#pragma once
#include "Application.h"
class GraphicsTut6 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	//2D Plane Buffers
	void CreatePlaneOGL_Buffers();
	void CreatePlaneOGL_Buffers_wNormals();
	unsigned int m_VAO;
	unsigned int m_VBO;
	unsigned int m_IBO;

	//models
	int BunnyModel;
	int SpearModel;

	//shaders
	int PointLight;
	int DirectionalLight;
	int Textured;
	int Point_Textured;
	int Point_Textured_bump;

	//Textures
	int BoxTex;


	GraphicsTut6();
	~GraphicsTut6();
};

