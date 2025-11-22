#pragma once
#include <vector>
#include "VoronoiContainers.h"

class GA_OldBuilding
{
public:
	std::vector<VoronoiBoxContainer*> Boxs;

	void GenBuilding(vec3 Min, vec3 Max, float RoofIndent, float RoofRowSpacing, float RoofRowSize, vec3 DoorLocation,vec3 DoorSize);
	void GenBuilding(int Xunits, int Zunits, float UnitSize, float Height, float RoofIndent, float RoofRowSpacing, float RoofRowSize, int DoorX, int DoorZ, float DoorMin, float DoorMax, float WallWidth,int Seeds,std::vector<glm::vec4> CutSpheres);
	
	void ApplyCuts(std::vector<glm::vec4> CutSpheres);

	void Draw();
	void DrawSpacers();

	GA_OldBuilding();
	~GA_OldBuilding();

	int SpacersAt;
};

