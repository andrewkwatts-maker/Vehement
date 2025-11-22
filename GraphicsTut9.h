#pragma once
#include "Application.h"
class GraphicsTut9: public Application
{
public:

	bool update() override;
	void draw() override;
	bool startup() override;

	float* PerlinData;

	void GenPerlin();
	void GenTerrain();

	int XScale = 100;
	int YScale = 100;
	float Scale = 1.0f;
	float Height = 12.0f;
	int Elliments = XScale*YScale;
	int Indexs = (XScale - 1)*(YScale - 1) * 6;
	int Octaves = 6;


	int PointTexturedBump;

	//Textures
	int GrassDiffuse;
	int GrassNormal;

	int TestGeometry;

	unsigned int VBO;
	unsigned int VAO;
	unsigned int IBO;

	GraphicsTut9();
	~GraphicsTut9();
};

