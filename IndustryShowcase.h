#pragma once
#include "Application.h"
#include "GL_Manager.h"
#include "AntTweak\AntTweakBar.h"

class IndustryShowcase :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	int CustomPlain;
	int TextureDiffuse;
	int	TextureNormal;
	int TextureHeightmap;
	int TextureRoughness;
	int TextureWaterDiffuse;
	int TextureWaterNormal;

	int ShaderProgram;

	IndustryShowcase();
	~IndustryShowcase();
private:
	float D_Steps;
	float W_Level;
	float L_Pow;

	vec3 LightLoc;

	TwBar* Menu;
};

