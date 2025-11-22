#include "Mathamatics3D.h"
#include <minmax.h>

Plane_3D::Plane_3D()
{

}

Plane_3D::Plane_3D(vec3 _PointOnPlane, vec3 _Normal)
{
	PointOnPlane = _PointOnPlane;
	if (_Normal != vec3(0))
		Normal = glm::normalize(_Normal);
	else
		Normal = _Normal;

	//d = -(ax0 + by0 + cz0)
	d = -(Normal.x*PointOnPlane.x + Normal.y*PointOnPlane.y + Normal.z*PointOnPlane.z);
}

bool Plane_3D::IsPointUnder(vec3 Point)
{
	//Make point once relitive to plane, relitive to origin
	//Point -= PointOnPlane;
	//
	//float Value = glm::dot(Point, Normal);
	//if (glm::dot(Point, Normal) > 0)
	//{
	//	if (Normal != vec3(0))
	//	{
	//		return false;
	//	}
	//	else
	//	{
	//		return true;
	//	}
	//}
	//else
	//{
	//	return true;
	//}

	if (Normal == vec3(0))
		return false;

	vec3 Inside = PointOnPlane - Normal;


	float Result = Normal.x*Point.x + Normal.y*Point.y + Normal.z*Point.z + d;
	float InsideResult = Normal.x*Inside.x + Normal.y*Inside.y + Normal.z*Inside.z + d;

	if ((Result <= 0 && InsideResult <= 0) || (Result >= 0 && InsideResult >= 0))
	{
		return true;
	}
	else
	{
		return false;
	}



};

Line_3D Plane_3D::GetInterceptLine(Plane_3D OtherPlane)
{

	vec3 CrossResult = glm::cross(Normal, OtherPlane.Normal);
	bool Fail = false;
	bool CheckA1Pass = false;
	bool CheckB1Pass = false;
	bool CheckC1Pass = false;
	bool CheckA2Pass = false;
	bool CheckB2Pass = false;
	bool CheckC2Pass = false;

	if (CrossResult == vec3(0))
	{
		//Parrallell = no intercept
		Fail = true;
	}

	float d1 = d;
	float d2 = OtherPlane.d;

	float a1 = Normal.x;
	float a2 = OtherPlane.Normal.x;

	float b1 = Normal.y;
	float b2 = OtherPlane.Normal.y;

	float c1 = Normal.z;
	float c2 = OtherPlane.Normal.z;

	float x;
	float y;
	float z;

	if (b1 != 0)
		if ((c2 - c1*(b2 / b1)) != 0)
			CheckA1Pass = true;

	if (a1 != 0)
		if ((c2 - c1*(a2 / a1)) != 0)
			CheckB1Pass = true;

	if (a1 != 0)
		if ((b2 - b1*(a2 / a1)) != 0)
			CheckC1Pass = true;

	if (c1 != 0)
		if ((b2 - b1*(c2 / c1)) != 0)
			CheckA2Pass = true;

	if (c1 != 0)
		if ((b2 - b1*(a2 / a1)) != 0)
			CheckB2Pass = true;

	if (b1 != 0)
		if ((a2 - a1*(b2 / b1)) != 0)
			CheckC2Pass = true;

	if (Fail == false)
	{
		if (CheckA1Pass)
		{
			x = 0;
			z = ((b2 / b1)*d1 - d2) / (c2 - c1*(b2 / b1));
			y = -(c1*z + d1) / b1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}

		if (CheckB1Pass)
		{
			y = 0;
			z = ((a2 / a1)*d1 - d2) / (c2 - c1*(a2 / a1));
			x = -(c1*z + d1) / a1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}

		if (CheckC1Pass)
		{
			z = 0;
			y = ((a2 / a1)*d1 - d2) / (b2 - b1*(a2 / a1));
			x = -(b1*y + d1) / a1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}

		if (CheckA2Pass)
		{
			x = 0;
			y = ((c2 / c1)*d1 - d2) / (b2 - b1*(c2 / c1));
			z = -(b1*y + d1) / c1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}

		if (CheckB2Pass)
		{
			y = 0;
			x = ((c2 / c1)*d1 - d2) / (a2 - a1*(c2 / c1));
			z = -(a1*x + d1) / c1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}

		if (CheckC2Pass)
		{
			z = 0;
			x = ((b2 / b1)*d1 - d2) / (a2 - a1*(b2 / b1));
			y = -(a1*x + d1) / b1;
			return Line_3D(vec3(x, y, z), CrossResult);
		}
	}

	//Failure - No Intercept Available via this equation;
	return Line_3D(vec3(0), vec3(0));
}

float Plane_3D::GetIntercept(Line_3D Line)
{
	float Dot = glm::dot(Line.Direction, Normal);
	if (Dot != 0)
	{
		return (-d - glm::dot(Normal, Line.PointOnLine)) / Dot;
	}
	return FLT_MAX;
}

Line_3D::Line_3D()
{

}

Line_3D::Line_3D(vec3 _PointOnLine, vec3 _Direction)
{
	PointOnLine = _PointOnLine;

	if (_Direction != vec3(0))
	{
		Direction = glm::normalize(_Direction);
		//if (glm::dot(Direction, vec3(0, 1, 0)) < 0)
			//Direction = -Direction;
	}
	else
	{
		Direction = _Direction;
	}

}

float Line_3D::GetTValue(vec3 Point)
{
	//POINT = (POL) + T*(DIR)
	//POINT - POL = T*(DIR);
	//(POINT - POL)/DIR = T;

	if (Direction != vec3(0))
	{
		vec3 Delta = Point - PointOnLine;
		float MaxInDelta = 0;
		float MaxDeltaScale = 0;
		if (abs(Delta.x) > MaxDeltaScale)
		{
			MaxInDelta = Delta.x;
			MaxDeltaScale = abs(Delta.x);
		}
		if (abs(Delta.y) > MaxDeltaScale)
		{
			MaxInDelta = Delta.y;
			MaxDeltaScale = abs(Delta.y);
		}
		if (abs(Delta.z) > MaxDeltaScale)
		{
			MaxInDelta = Delta.z;
			MaxDeltaScale = abs(Delta.z);
		}


		float MaxInDir = 0;
		float MaxDirScale = 0;
		if (abs(Direction.x) > MaxDirScale)
		{
			MaxInDir = Direction.x;
			MaxDirScale = abs(Direction.x);
		}
		if (abs(Direction.y) > MaxDirScale)
		{
			MaxInDir = Direction.y;
			MaxDirScale = abs(Direction.y);
		}
		if (abs(Direction.z) > MaxDirScale)
		{
			MaxInDir = Direction.z;
			MaxDirScale = abs(Direction.z);
		}


		return MaxInDelta / MaxInDir;
	}
	else
	{
		return FLT_MAX; //NO T AVAILIBLE; LINE NOT PROPERLY MADE
	}

}


vec3 Line_3D::ClossestPointTo(Line_3D Other)
{
	vec3 thisPoint = PointOnLine;
	vec3 OtherPoint = Other.PointOnLine;

	vec3 d1 = Direction;
	vec3 d2 = Other.Direction;

	float a = glm::dot(d1, d1);
	float b = glm::dot(d1, d2);
	float e = glm::dot(d2, d2);

	float d = a*e - b*b;

	if (d != 0)
	{
		vec3 r = thisPoint - OtherPoint;
		float c = glm::dot(d1, r);
		float f = glm::dot(d2, r);

		float s = (b*f - c*e) / d;
		float t = (a*f - b*c) / d;

		thisPoint = PointOnLine + Direction*s;
		OtherPoint = Other.PointOnLine + Other.Direction*t;
		return thisPoint;
	}
	else
	{
		return vec3(FLT_MAX);
	}

}

#include <cfloat>

vec3 Line_3D::Intercept(Line_3D Other) //Nolonger used i tihnk.
{
	//p0x = point on this line x position, p0y = point on this line y position, p1x = point on other line x position, p1y = point on other line y position, d0x = direction of this line x componant.. ect ect. m = multiple of this lines direction vector, n = multiple of other lines direction vector

	//p0x +d0x*m = p1x+d1x*n

	//m = (p1x+d1x*n - p0x)/d0x;

	//p0y +d0y*m = p1y+d1y*n

	//m = (p1y +d1y*n - p0y)/d0y

	//(p1x+d1x*n - p0x)/d0x = (p1y +d1y*n - p0y)/d0y;

	//(p1x+d1x*n - p0x) = d0x(p1y +d1y*n - p0y)/d0y;

	//n = (-d0y*p0x +p0y*d0x -d0x*p1y +d0y*p1x)/(d0x*d1y-d0y*d1x) d0x!=0 && (d0x*d1y-d0y*d1x) != 0

	float n;
	float Div = Direction.x*Other.Direction.y - Direction.y*Other.Direction.x;

	if (Div != 0 && Other.Direction != vec3(0) && Direction != vec3(0))
	{
		n = (-Direction.y*PointOnLine.x + PointOnLine.y*Direction.x - Direction.x*Other.PointOnLine.y + Direction.y*Other.PointOnLine.x) / Div;
	}
	else
	{
		if (glm::dot(Direction, Other.Direction) == 0) //RightAngle to Each Other
		{
			if (max(max(Direction.x*Direction.x, Direction.y*Direction.y), Direction.z*Direction.z) == 1 && max(max(Other.Direction.x*Other.Direction.x, Other.Direction.y*Other.Direction.y), Other.Direction.z* Other.Direction.z) == 1)
			{
				vec3 Result = Direction*Direction*Other.PointOnLine + Other.Direction*Other.Direction*PointOnLine + (vec3(1) - (Direction + Other.Direction)*(Direction + Other.Direction))*PointOnLine;
				return Result;
			}
			else
			{
				// Error Input
				return vec3(FLT_MAX);
				n = FLT_MAX;
			}
		}
		else
		{
			// Parrallel Or Error Input
			return vec3(FLT_MAX);
			n = FLT_MAX;
		}
	}
	return PointOnLine + Direction*n;


}

vec3 Line_3D::GetPointFromT(float T)
{
	return PointOnLine + T*Direction;
}