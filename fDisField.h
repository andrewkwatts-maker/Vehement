#pragma once
#include "Application.h"
#include "AntTweakBar.h"
class fDisField : public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	int CameraLensePlane;
	int fDistanceShader;

	std::string VS_S1;
	std::string FS_S1;
	int fFieldLoc; //Location to Apply ShaderCode

	TwBar* Menu;
	float Tilt;
	float FOV;
	float Lensing;
	int MaxSteps;

	fDisField();
	~fDisField();
};

