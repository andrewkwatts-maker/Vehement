#pragma once
#include "glm\glm.hpp"
using glm::vec3;


class Line_3D //Infinite length;
{
public:
	Line_3D();
	Line_3D(vec3 _PointOnLine, vec3 _Direction);

	vec3 ClossestPointTo(Line_3D Other);
	vec3 Intercept(Line_3D OtherLine); //Lines must sit on same plane
	float GetTValue(vec3 Point); //Point Must be on same Line;
	vec3 GetPointFromT(float T);

	vec3 PointOnLine;
	vec3 Direction;
};


class Plane_3D //Infinite bounds
{
public:
	Plane_3D();
	Plane_3D(vec3 _PointOnPlane, vec3 _Normal);

	bool IsPointUnder(vec3 Point); // bellow when not on the same side as the normal vector.
	Line_3D GetInterceptLine(Plane_3D OtherPlane); // returns the line in 3d of that describes where two planes intercept, returns a line with constructor Line3D(vec3(0),vec3(0)) if the planes are parrallel.

	float GetIntercept(Line_3D Line); //float is the Line Normal Multiple that gets to the intersection from point on line.

	vec3 PointOnPlane; //(x0,y0,z0)
	vec3 Normal; //(a,b,c)

	//a(x-x0) + b(y-y0)+c(z-z0) = 0
	//ax+by+cz +d = 0 where d = -(ax0+by0+cz0)
	float d;

};