#include "NODES_BoundingSphere.h"

void NODES_BoundingSphere::fit(const NODES_BoundingSphere& sphere)
{
	float d = glm::distance(centre, sphere.centre) + sphere.radius;
	if (d > radius)
		radius = d;
}


NODES_BoundingSphere::NODES_BoundingSphere()
{
	centre = glm::vec4(0);
	radius = 1;
}


NODES_BoundingSphere::~NODES_BoundingSphere()
{
}
