#pragma once
#include "Application.h"
class GraphicsTut14 :
	public Application
{
public:

	bool update() override;
	void draw() override;
	bool startup() override;


	//models
	int BunnyModel;
	int SpearModel;
	int Ground;

	//shaders
	int PointLight;
	int DirectionalLight;
	int Textured;
	int Point_Textured;
	int Point_Textured_bump;

	//Textures
	int GroundTex;
	int GroundN;

	GraphicsTut14();
	~GraphicsTut14();
};

