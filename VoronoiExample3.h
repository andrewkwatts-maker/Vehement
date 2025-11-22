#pragma once
#include "Application.h"
#include "VoronoiMathamatics.h"
#include "VoronoiContainers.h"
class VoronoiExample3:public Application
{
public:
	VoronoiCell* Cell;
	VoronoiBoxContainer* Box;

	bool update() override;
	void draw() override;
	bool startup() override;

	int PointTexturedBump;
	int RockDiffuse;
	int RockNormal;


	VoronoiExample3();
	~VoronoiExample3();
};

