#pragma once
#include <vector>
#include "VoronoiMathamatics.h"
class VoronoiBoxContainer
{
public:
	VoronoiBoxContainer(vec3 Min, vec3 Max);
	~VoronoiBoxContainer();

	void DeleteCells();
	std::vector<VoronoiCell*> Cells;
	vec3 Min;
	vec3 Max;
	void AddRandomSeed(VoroType TypeRef);
	void CalculateAllSeeds();
	void SetInsideSphereToType(VoroType TypeRef, vec3 Center, float Radius);
	void Draw();

	void DrawEdges(vec4 Col);
};
