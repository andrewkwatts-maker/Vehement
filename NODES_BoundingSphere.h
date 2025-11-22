#pragma once
#include <glm\glm.hpp>
class NODES_BoundingSphere
{
public:
	NODES_BoundingSphere();
	~NODES_BoundingSphere();

	void fit(const NODES_BoundingSphere& sphere);

	glm::vec4 centre;
	float radius;
};

