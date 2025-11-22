#pragma once
#include "Application.h"
class GraphicsTut6a :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	void CreatePlaneOGL_Buffers();
	unsigned int m_VAO;
	unsigned int m_VBO;
	unsigned int m_IBO;

	int PointTexturedBump;
	int BoxTexture;
	int RockDiffuse;
	int RockNormal;
	int RockWallDiffuse;
	int RockWallNormal;
	int TilesDiffuse;
	int TilesNormal;
	int GrassDiffuse;
	int GrassNormal;

	int SpecTexProgram;
	int Spear;
	int SpearTex;


	GraphicsTut6a();
	~GraphicsTut6a();
};

