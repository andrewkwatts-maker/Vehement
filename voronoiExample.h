#pragma once
#include "Application.h"
#include "VORO_MATHS.h"
#include "VORO_CELL.h"
class voronoiExample :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	VORO_CELL_CALCULATOR* CELL;
	VORO_CONTAINER* Cont;

	VORO_CELL* STD_CELL;

	vector<VORO_SEED*> SEEDS;
	vector<VORO_CELL*> CELLS;
	vector<bool> Visible;

	int Seeds;
	vec3 Base;
	float S;


	//GLData
	int PointTexturedBump;
	int RockDiffuse;
	int RockNormal;

	voronoiExample();
	~voronoiExample();
};

