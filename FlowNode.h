#pragma once
#include "glm\vec3.hpp"

class FlowNode
{
public:
	glm::vec3 Position;
	int Score;
	bool Wall;
	bool Visited;
	FlowNode* Edges[4];

	FlowNode();
	~FlowNode();
};

class FlowBot
{
public:
	glm::vec3 Position;
	glm::vec3 Dir;
};
