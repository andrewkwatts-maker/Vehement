#pragma once
#include "GA_OldBuilding.h"
#include <vector>
#include "GL_Manager.h"
class GA_BuildingCluster
{
public:
	void Draw(vec3 LOC,GL_Manager* Manager);
	void DrawSpacers(vec3 LOC, GL_Manager* Manager);
	vec4 Build(float Age,int Fractures); //Age Between 0 and 1, vec4 is size of base, and loc of door.
	void ApplyAge(float Age, float RoofHeight, int Seed, int Xsize, int Ysize);
	vec4 WholeBuild();
	vec4 Details;
	GA_BuildingCluster();
	~GA_BuildingCluster();
private:
	std::vector<GA_OldBuilding*> Cluster;
	VoronoiBoxContainer* Base;
	std::vector<vec3> Locs;
};

