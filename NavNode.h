#pragma once
#include <glm\vec3.hpp>



class NavNode
{
public:
	glm::vec3 position;
	glm::vec3 vertices[3];
	NavNode* edgeTargets[3];

	unsigned int flags;
	float edgeCosts[3];

	NavNode();
	~NavNode();
};

