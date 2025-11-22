#include "VORO_MATHS.h"
#include "glm\glm.hpp"
#include <aie\Gizmos.h>
#include <minmax.h>
#include "VORO_CELL.h"

int vHitE1;
int vHitE2;
int vHitE3;
int vHitE4;
int vHitE5;
int vHitE6;
int vHitE7;
int vHitE8;
int vHitE9;
int vHitE10;
int vHitE11;
int vHitE12;
int vHitE13;
int vHitE14;
int vHitE15;
int vHitE16;


Plane3D::Plane3D(vec3 _PointOnPlane, vec3 _Normal)
{
	PointOnPlane = _PointOnPlane;
	Normal = glm::normalize(_Normal);

	//d = -(ax0 + by0 + cz0)
	d = -(Normal.x*PointOnPlane.x + Normal.y*PointOnPlane.y + Normal.z*PointOnPlane.z);
}

bool Plane3D::IsPointUnder(vec3 Point)
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

Line3D Plane3D::GetIntercept(Plane3D OtherPlane)
{

	vec3 CrossResult = glm::cross(Normal, OtherPlane.Normal);
	bool Fail = false;
	bool CheckA1Pass = false;
	bool CheckB1Pass = false;
	bool CheckC1Pass = false;
	bool CheckA2Pass = false;
	bool CheckB2Pass = false;
	bool CheckC2Pass = false;
	bool CheckA1Pass_INV = false;  //InverseChecks
	bool CheckB1Pass_INV = false;  //InverseChecks
	bool CheckC1Pass_INV = false;  //InverseChecks
	bool CheckA2Pass_INV = false;  //InverseChecks
	bool CheckB2Pass_INV = false;  //InverseChecks
	bool CheckC2Pass_INV = false;  //InverseChecks


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

	//First Check

	//a1x + b1y +c1z +d1 = a2x +b2y+ c2z +d2 = 0
	//sub x = 0;
	//b1y +c1z +d1 = b2y+c2z+d2 = 0
	//multiply by (-b2/b1)

	//-b2y + (-b2/b1)*c1z + (-b2/b1)*d1 = 0
	//b2y +c2z +d2 = 0

	//add together

	//(c2 + (-b2/b1)*c1)*z +(-b2/b1)*d1 = 0
	//z = ((b2/b1)*d1) / ( (-b2/b1)*c1 +c2)

	//sub z into earlier equation b1y +c1z +d1 = 0
	// y = -(c1*z +d1)/b1;

	//==============================================
	//==============================================
	//==============================================

	//Secound Check

	//a1x + b1y +c1z +d1 = a2x +b2y+ c2z +d2 = 0
	//sub y = 0;
	//a1x + c1z +d1 = a2x + c2z +d2 = 0
	//multiply by (-a2/a1)

	//-a2x + (-a2/a1)*c1z + (-a2/a1)*d1 = 0
	//a2x + c2z +d2 = 0

	//add together

	// (c2 + (-a2/a1)*c1))*z + (-a2/a1)*d1 + d2 = 0
	// z = ((a2/a1)*d1 -d2) / (c2 - c1*(a2/a1))

	//sub z into earlier equation a1x + c1z +d1 = 0
	// x = -(c1z +d1)/a1;


	//==============================================
	//==============================================
	//==============================================

	//Third Check

	//a1x + b1y +c1z +d1 = a2x +b2y+ c2z +d2 = 0
	//sub z = 0;
	//a1x + b1y +d1 = a2x + b2y +d2 = 0
	//multiply by (-a2/a1)

	//-a2x + (-a2/a1)*b1y + (-a2/a1)*d1 = 0
	// a2x + b2y +d2 = 0

	//add together

	// (b2 + (-a2/a1)*b1))*y + (-a2/a1)*d1 + d2 = 0
	// y = ((a2/a1)*d1 - d2) / (b2 -b1*(a2/a1))


	//sub y into earlier equation a1x + b1y +d1 = 0
	// x = -(b1y +d1)/a1;


	//RegularChecks
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

	//InverseChecks
	if (b2 != 0)
		if ((c1 - c2*(b1 / b2)) != 0)
			CheckA1Pass_INV = true;

	if (a2 != 0)
		if ((c1 - c2*(a1 / a2)) != 0)
			CheckB1Pass_INV = true;

	if (a2 != 0)
		if ((b1 - b2*(a1 / a2)) != 0)
			CheckC1Pass_INV = true;

	if (c2 != 0)
		if ((b1 - b2*(c1 / c2)) != 0)
			CheckA2Pass_INV = true;

	if (c2 != 0)
		if ((b1 - b2*(a1 / a2)) != 0)
			CheckB2Pass_INV = true;

	if (b2 != 0)
		if ((a1 - a2*(b1 / b2)) != 0)
			CheckC2Pass_INV = true;





	if (Fail == false)
	{
		if (CheckA1Pass)
		{
			x = 0;
			z = ((b2 / b1)*d1 - d2) / (c2 - c1*(b2 / b1));
			y = (-c1*z - d1) / b1;
			return Line3D(vec3(x, y, z), -CrossResult);
		}

		if (CheckB1Pass)
		{
			y = 0;
			z = ((a2 / a1)*d1 - d2) / (c2 - c1*(a2 / a1));
			x = -(c1*z + d1) / a1;
			return Line3D(vec3(x, y, z), -CrossResult);
		}

		if (CheckC1Pass)
		{
			z = 0;
			y = ((a2 / a1)*d1 - d2) / (b2 - b1*(a2 / a1));
			x = -(b1*y + d1) / a1;
			return Line3D(vec3(x, y, z),-CrossResult);
		}

		if (CheckA2Pass)
		{
			x = 0;
			y = ((c2 / c1)*d1 - d2) / ((-c2 / c1)*b1 + b2);
			z = -(b1*y+d1)/c1;
			return Line3D(vec3(x, y, z), -CrossResult);
		}
		
		if (CheckB2Pass)
		{
			y = 0;
			x = ((c2 / c1)*d1 - d2) / (a2 - (c2 / c1)*a1);
			z = -(a1*x + d1) / c1;
			return Line3D(vec3(x, y, z), -CrossResult);
		}
		
		if (CheckC2Pass)
		{
			z = 0;
			x = ((b2 / b1)*d1 - d2) / (a2 - a1*(b2 / b1));
			y = -(a1*x + d1) / b1;
			return Line3D(vec3(x, y, z), -CrossResult);
		}

		//InverseChecks

		if (CheckA1Pass_INV)
		{
			x = 0;
			z = ((b1 / b2)*d2 - d1) / (c1 - c2*(b1 / b2));
			y = (-c2*z - d2) / b2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

		if (CheckB1Pass_INV)
		{
			y = 0;
			z = ((a1 / a2)*d2 - d1) / (c1 - c2*(a1 / a2));
			x = -(c2*z + d2) / a2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

		if (CheckC1Pass_INV)
		{
			z = 0;
			y = ((a1 / a2)*d2 - d1) / (b1 - b2*(a1 / a2));
			x = -(b2*y + d2) / a2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

		if (CheckA2Pass_INV)
		{
			x = 0;
			y = ((c1 / c2)*d2 - d1) / ((-c1 / c2)*b1 + b2);
			z = -(b2*y + d2) / c2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

		if (CheckB2Pass_INV)
		{
			y = 0;
			x = ((c1 / c2)*d2 - d1) / (a1 - (c1 / c2)*a2);
			z = -(a2*x + d2) / c2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

		if (CheckC2Pass_INV)
		{
			z = 0;
			x = ((b1 / b2)*d2 - d1) / (a1 - a2*(b1 / b2));
			y = -(a2*x + d2) / b2;
			return Line3D(vec3(x, y, z), CrossResult);
		}

	}

	//Failure - No Intercept Available via this equation;
	return Line3D(vec3(0), vec3(0));

	


}

Line3D::Line3D(vec3 _PointOnLine, vec3 _Direction)
{
	PointOnLine = _PointOnLine;

	if (_Direction != vec3(0))
	{
		Direction = glm::normalize(_Direction);
	}
	else
	{
		Direction = _Direction;
	}
		
}

float Line3D::GetTValue(vec3 Point)
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

vec3 Line3D::ClossestPointTo(Line3D Other,vec3 &thisPoint,vec3 &OtherPoint) //DO NOT USE - CHECK IF CAN BE REMOVED
{
	thisPoint = PointOnLine;
	OtherPoint = Other.PointOnLine;

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
		//Lines were Parrallel
	}

}


vec3 Line3D::ClossestPointTo(Line3D Other)
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

vec3 Line3D::Intercept(Line3D Other) //Nolonger used i tihnk.
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
		if (glm::dot(Direction,Other.Direction)==0) //RightAngle to Each Other
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


//VoroSeed
VORO_SEED::VORO_SEED(vec3 Loc, float _Scale)
{
	Location = Loc;

	if (_Scale < 0) //must be possitive
	{
		_Scale = 1;
	}
	Scale = _Scale;
	Type = VoroType::eEmpty;
};
VORO_SEED::VORO_SEED(vec3 Loc)
{
	Location = Loc;
	Scale = 1;
	Type = VoroType::eEmpty;
};
VORO_SEED::VORO_SEED()
{
	Scale = 1;
	Type = VoroType::eEmpty;
};

//================================================================================

//VORO_CONSTRUCT_CLASS

VORO_CELL_CALCULATOR::VORO_CELL_CALCULATOR(vec3 Loc, float _Scale, VoroType type)
{
	Location = Loc;
	Scale = _Scale;
	BoundingRadius = FLT_MAX;
	Type = type;
}

VORO_CELL_CALCULATOR::~VORO_CELL_CALCULATOR()
{
	for (int Face = 0; Face < Faces.size(); Face++)
	{
		delete Faces[Face];
	}
	Faces.empty();
	//SEED = nullptr;
}

void VORO_CELL_CALCULATOR::AddSeed(VORO_SEED* SEED_REF)
{
	VORO_CELL_FACE* FaceID = new VORO_CELL_FACE(this, SEED_REF);
	float Ratio = SEED_REF->Scale*(1 / this->Scale);
	float LocationRatio = 1 / (1 + Ratio);
	float PointRadius = glm::length(SEED_REF->Location - Location)*LocationRatio;


	if (PointRadius < BoundingRadius) //Check Its Needed - NEEDS FIXING FOR SCALE REASONS
	{
		for (int Face = 0; Face < Faces.size(); ++Face)
		{
			VORO_CELL_EDGE* New = new VORO_CELL_EDGE(FaceID, Faces[Face]);
			if (New->Line->Direction != vec3(0))
			{
				++vHitE1;
				FaceID->Edges.push_back(New);
			}
			else
			{
				++vHitE2;
				delete New;
			}
		}

		//Edges
		for (int Edge1 = 0; Edge1 < FaceID->Edges.size(); ++Edge1)
		{
			for (int Edge2 = 0; Edge2 < FaceID->Edges.size(); ++Edge2)
			{
				if (Edge1 != Edge2 )
				{
					++vHitE3;
					FaceID->Edges[Edge2]->CheckIntercept_wEdge(FaceID->Edges[Edge1]);
				}
				else
				{
					++vHitE4;
				}
			}
		}

		//Check If Has A Valid Edge On New Face
		int SumValid = 0;
		for (int Edge1 = 0; Edge1 < FaceID->Edges.size(); ++Edge1)
		{
			if (FaceID->Edges[Edge1]->IsValidEdge == true)
			{
				++SumValid;
				++vHitE5;
				Edge1 = FaceID->Edges.size(); //BreakLoop
			}
			{
				++vHitE6;
			}
		}

		if (SumValid == 0)
		{
			++vHitE7;
			delete FaceID;
		}
		else
		{
			//DeleteInvalid from FaceID
			for (int Edge1 = 0; Edge1 < FaceID->Edges.size(); ++Edge1)
			{
				if (FaceID->Edges[Edge1]->IsValidEdge == false)
				{
					++vHitE8;
					FaceID->Edges.erase(FaceID->Edges.begin() + Edge1);
					--Edge1;
				}
				else
				{
					++vHitE9;
				}
			}

			for (int Face = 0; Face < Faces.size(); ++Face)
			{
				VORO_CELL_EDGE* New = new VORO_CELL_EDGE(Faces[Face], FaceID);
				if (New->Line->Direction != vec3(0))
				{
					++vHitE10;
					for (int Edge1 = 0; Edge1 < Faces[Face]->Edges.size(); ++Edge1)
					{
						Faces[Face]->Edges[Edge1]->CheckIntercept_wEdge(New);
						New->CheckIntercept_wEdge(Faces[Face]->Edges[Edge1]);
					}
					Faces[Face]->Edges.push_back(New);
				}
				else
				{
					++vHitE11;
					delete New;
				}
			}

			



			////CalculateOtherFaces Intercepts with FaceID
			//for (int Edge1 = 0; Edge1 < FaceID->Edges.size(); ++Edge1 /*int Face = 0; Face < Faces.size();++Face*/)
			//{
			//	VORO_CELL_FACE* FaceRef = FaceID->Edges[Edge1]->FormingFace_Other; //Checks Intercepts with Valid Edges; /*Faces[Face]*/;
			//	VORO_CELL_EDGE* New = new VORO_CELL_EDGE(FaceRef, FaceID);
			//	if (New->Line->Direction != vec3(0))
			//	{
			//		int InsertPos = FaceRef->Edges.size();
			//		FaceRef->Edges.push_back(New);
			//		for (int Edge2 = 0; Edge2 < InsertPos; ++Edge2)
			//		{
			//			FaceRef->Edges[Edge2]->CheckIntercept_wEdge(New);
			//			New->CheckIntercept_wEdge(FaceRef->Edges[Edge2]);
			//
			//		}
			//
			//
			//		//Delete Invalid
			//		int Sum = 0;
			//		for (int Edge2 = 0; Edge2 < FaceRef->Edges.size(); ++Edge2)
			//		{
			//			if (FaceRef->Edges[Edge2]->IsValidEdge == true)
			//			{
			//				++Sum;
			//				Edge2 = FaceRef->Edges.size(); //BreakLoop
			//			}
			//		}
			//
			//		if (Sum == 0)
			//		{
			//			delete FaceRef;
			//		}
			//		else
			//		{
			//			for (int Edge2 = 0; Edge2 < FaceRef->Edges.size(); ++Edge2)
			//			{
			//				if (FaceRef->Edges[Edge2]->IsValidEdge == false)
			//				{
			//					FaceRef->Edges.erase(FaceRef->Edges.begin() + Edge2);
			//					--Edge2;
			//				}
			//			}
			//		}
			//	}
			//	else
			//	{
			//		delete New;
			//	}
			//}
			//Cull Issolated Faces


			GenBoundingRadius(); // Optimise to only update when their is a change (through Intercept With Edge Fucntion Call).
			//AddFace
			Faces.push_back(FaceID);
		}
	}
	else
	{
		++vHitE12;
		delete FaceID;
	}

	for (int Face = 0; Face < Faces.size(); ++Face) //Why Cant this be optimised - Keep a tally of edges, and subtract from a tally of Acceptable Edges ussing Intercept w Function
	{
		int Sum = 0;
		for (int Edge = 0; Edge < Faces[Face]->Edges.size();++Edge)
		{
			if (Faces[Face]->Edges[Edge]->IsValidEdge)
			{
				++Sum;
				++vHitE13;
				Edge = Faces[Face]->Edges.size();
			}
			else //Added Recently, Untested
			{
				//delete Faces[Face]->Edges[Edge];
				//Faces[Face]->Edges.erase(Faces[Face]->Edges.begin() + Edge);
				//--Edge;
				//++vHitE14;
			}
		}
		if (Sum == 0)
		{
			++vHitE15;
			//delete Faces[Face];
			Faces.erase(Faces.begin() + Face);
			//Face--;
		}
		else
		{
			++vHitE16;
		}
	}
}

void VORO_CELL_CALCULATOR::AddSeedOLD(VORO_SEED* SEED_REF)
{
	VORO_CELL_FACE* FaceID = new VORO_CELL_FACE(this, SEED_REF);
	Faces.push_back(FaceID);
}

void VORO_CELL_CALCULATOR::CalculateFromFaces()
{
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		for (int Face2 = 0; Face2 < Faces.size(); ++Face2) //Face we are comparing with
		{
			if (Face != Face2)
			{
				VORO_CELL_EDGE* New = new VORO_CELL_EDGE(Faces[Face], Faces[Face2]);
				if (New->Line->Direction != vec3(0))
					Faces[Face]->Edges.push_back(New);
				else
					delete New;
			}
		}

		//Edges
		for (int Edge1 = 0; Edge1 < Faces[Face]->Edges.size(); ++Edge1)
		{
			for (int Edge2 = 0; Edge2 < Faces[Face]->Edges.size(); ++Edge2)
			{
				if (Edge1 != Edge2)
				{
					//if (Edge1 == 5 && Edge2 == 4 && Face == 4)
					//{
					//	bool ShouldCreateNewLimit = true;
					//}

					Faces[Face]->Edges[Edge1]->CheckIntercept_wEdge(Faces[Face]->Edges[Edge2]);
				}
			}
		}
	}

	//EdgeCull - Demonstrates a LARGE Error in Respective Face Locations
	//for (int Face = 0; Face < Faces.size(); ++Face)
	//{
	//	for (int Edge1 = 0; Edge1 < Faces[Face]->Edges.size(); ++Edge1)
	//	{
	//		bool FailedToFindPos = true;
	//		bool FailedToFindNeg = true;
	//		for (int Edge2 = 0; Edge2 < Faces[Face]->Edges.size(); ++Edge2)
	//		{
	//			if (Edge1 != Edge2)
	//			{
	//				vec3 DeltaPosPos = Faces[Face]->Edges[Edge1]->BoundPos - Faces[Face]->Edges[Edge2]->BoundPos;
	//				vec3 DeltaPosNeg = Faces[Face]->Edges[Edge1]->BoundPos - Faces[Face]->Edges[Edge2]->BoundNeg;
	//				vec3 DeltaNegPos = Faces[Face]->Edges[Edge1]->BoundNeg - Faces[Face]->Edges[Edge2]->BoundPos;
	//				vec3 DeltaNegNeg = Faces[Face]->Edges[Edge1]->BoundNeg - Faces[Face]->Edges[Edge2]->BoundNeg;
	//
	//				float CutOff = pow(10, 35); // Why is the error so big? a tenth of 1%
	//				float ScaleError = CutOff*FLT_MIN;
	//
	//				if (DeltaPosPos == vec3(0) || DeltaPosNeg == vec3(0) || glm::length(DeltaPosPos) <= CutOff*FLT_MIN || glm::length(DeltaPosNeg) <= CutOff* FLT_MIN)
	//					FailedToFindPos = false;
	//				if (DeltaNegPos == vec3(0) || DeltaNegNeg == vec3(0) || glm::length(DeltaNegPos) <= CutOff*FLT_MIN || glm::length(DeltaNegNeg) <= CutOff* FLT_MIN)
	//					FailedToFindNeg = false;
	//
	//			}
	//		}
	//		if (FailedToFindPos || FailedToFindNeg)
	//		{
	//			Faces[Face]->Edges[Edge1]->IsValidEdge = false;
	//			delete Faces[Face]->Edges[Edge1];
	//			Faces[Face]->Edges.erase(Faces[Face]->Edges.begin() + Edge1);
	//			--Edge1;
	//		}
	//	}
	//}

	//CullFaces
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		if (Faces[Face]->Edges.size() == 0)
		{
			delete Faces[Face];
			Faces.erase(Faces.begin() + Face);
			--Face;
		}
	}

	//ReSizeVectors  = Memory Saving
	Faces.resize(Faces.size());
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		Faces[Face]->Edges.resize(Faces[Face]->Edges.size());
	}


}


void VORO_CELL_CALCULATOR::CleanUp()
{
	//CullEdges
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		for (int Edge = 0; Edge < Faces[Face]->Edges.size(); ++Edge)
		{
			for (int Edge2 = 0; Edge2 < Faces[Face]->Edges.size(); ++Edge2)
			{
				if (Edge != Edge2)
				{
					Faces[Face]->Edges[Edge]->CheckIntercept_wEdge(Faces[Face]->Edges[Edge2]);
				}
			}
		}
	}



	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		for (int Edge = 0; Edge < Faces[Face]->Edges.size(); ++Edge)
		{
			if (Faces[Face]->Edges[Edge]->IsValidEdge == false)
			{
				delete Faces[Face]->Edges[Edge];
				Faces[Face]->Edges.erase(Faces[Face]->Edges.begin() + Edge);
				--Edge;
			}
		}
	}
}

void VORO_CELL_CALCULATOR::GenBoundingBox(float Size)
{
	VORO_SEED* A = new VORO_SEED(this->Location + vec3(Size / 2, 0, 0));
	VORO_SEED* B = new VORO_SEED(this->Location + vec3(0, Size / 2, 0));
	VORO_SEED* C = new VORO_SEED(this->Location + vec3(0, 0, Size / 2));
	VORO_SEED* D = new VORO_SEED(this->Location + vec3(-Size / 2, 0, 0));
	VORO_SEED* E = new VORO_SEED(this->Location + vec3(0, -Size / 2, 0));
	VORO_SEED* F = new VORO_SEED(this->Location + vec3(0, 0, -Size / 2));

	this->AddSeedOLD(A);
	this->AddSeedOLD(B);
	this->AddSeedOLD(C);
	this->AddSeedOLD(D);
	this->AddSeedOLD(E);
	this->AddSeedOLD(F);
	this->CalculateFromFaces();

	for (int i = 0; i < Faces.size(); ++i)
	{
		Faces[i]->FormingSeed_Other = nullptr;
	}

	delete A;
	delete B;
	delete C;
	delete D;
	delete E;
	delete F;

	this->GenBoundingRadius();
}

void VORO_CELL_CALCULATOR::GenBoundingRadius()
{
	float LargestDistance = 0;
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		for (int Edge = 0; Edge < Faces[Face]->Edges.size(); ++Edge)
		{
			if (Faces[Face]->Edges[Edge]->IsValidEdge)
			{
				float Dist = max(glm::length(Location - Faces[Face]->Edges[Edge]->BoundPos), glm::length(Location - Faces[Face]->Edges[Edge]->BoundNeg));
				if (Dist > LargestDistance)
					LargestDistance = Dist;
			}
		}
	}
	BoundingRadius = LargestDistance;
}

void VORO_CELL_CALCULATOR::DrawEdges(vec3 Col)
{
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		int FaceN = Face;
		//Face = 1;
		for (int Edge = 0; Edge < Faces[Face]->Edges.size(); ++Edge)
		{
			int EdgeN = Edge;
			//Edge = 3;
			Faces[Face]->Edges[Edge]->Draw(Col);
			Edge = EdgeN;
		}
		Face = FaceN;
	}
}

VORO_CELL_FACE::VORO_CELL_FACE(VORO_SEED* This, VORO_SEED* Other)
{
	FormingSeed_Other = Other;
	FormingSeed_This = This;
	Face = GenPlane();
};

void VORO_CELL_FACE::FormEdge(VORO_CELL_FACE* Face_Ref)
{

	//Form New Edge - > only Place A New Edge should Be Formed;
	VORO_CELL_EDGE* EDGE_REF = new VORO_CELL_EDGE(this,Face_Ref);

	if (EDGE_REF->Line->Direction != vec3(0))
	{
		//Do Checks
		for (int Edge = 0; Edge < Edges.size(); ++Edge)
		{
			//CalculateIntercepts w Other Edges for Other Lines;
			Edges[Edge]->CheckIntercept_wEdge(EDGE_REF);
		}
		for (int Edge = 0; Edge < Edges.size(); ++Edge)
		{
			//CalculateIntercepts w Other Edges for This Line;
			EDGE_REF->CheckIntercept_wEdge(Edges[Edge]);
		}

		//AddEdge if Valid;
		if (EDGE_REF->IsValidEdge)
			Edges.push_back(EDGE_REF);
	}
	else
	{
		delete EDGE_REF;
	}
}

void VORO_CELL_FACE::RemoveUnvalidEdges()
{
	for (int Edge = 0; Edge < Edges.size(); ++Edge)
	{
		if (Edges[Edge]->IsValidEdge == false)
		{
			delete Edges[Edge];
			Edges.erase(Edges.begin() + Edge);
		}
	}
};

bool VORO_CELL_FACE::IsValidFace()
{
	RemoveUnvalidEdges();
	if (Edges.size() == 0)
		return false;
	else
		return true;
}

bool VORO_CELL_FACE::ContainsFaceRef(VORO_CELL_FACE* FACE_REF)
{
	for (int Edge = 0; Edge < Edges.size(); ++Edge)
	{
		if (Edges[Edge]->FormingFace_Other == FACE_REF || Edges[Edge]->FormingFace_This == FACE_REF)
			return true;
	}
	return false;
}

Plane3D* VORO_CELL_FACE::GenPlane()
{
	float Ratio = FormingSeed_Other->Scale*(1 / FormingSeed_This->Scale);
	float LocationRatio = 1 / (1 + Ratio);
	vec3 Direction = FormingSeed_Other->Location - FormingSeed_This->Location;

	vec3 Loc = FormingSeed_This->Location + Direction*LocationRatio;
	return new Plane3D(Loc, Direction);
};

VORO_CELL_FACE::~VORO_CELL_FACE()
{
	FormingSeed_Other = nullptr;
	FormingSeed_This = nullptr;
	delete Face;

	for (int Edge = 0; Edge < Edges.size(); Edge++)
	{
		delete Edges[Edge];
	}

}


void VORO_CELL_EDGE::CheckIntercept_wEdge(VORO_CELL_EDGE* Edge)
{
	float DeltaOffset = 200.0f;

	vec3 PointOfIntercept = Line->ClossestPointTo(Line3D(Edge->Line->PointOnLine, Edge->Line->Direction));
	if (PointOfIntercept != vec3(FLT_MAX) && Line->Direction != vec3(0)&& Edge->Line->Direction != vec3(0))
	{

		vec3 PosDelta = PointOfIntercept + Line->Direction * DeltaOffset;
		vec3 NegDelta = PointOfIntercept - Line->Direction * DeltaOffset;
		if (Edge->FormingFace_Other->Face->IsPointUnder(PosDelta))
		{
			//Positive Direction
			float NewT = Line->GetTValue(PointOfIntercept);
			if (bHasBoundPos == false)
			{
				BoundPos_TValue = NewT;
				BoundPos = PointOfIntercept;
				LineBoundingPos = Edge;
				bHasBoundPos = true;
			}
			else
			{
				if (NewT > BoundPos_TValue)
				{
					//CullOldData
					//if (LineBoundingPos->BoundNeg == PointOfIntercept)
					//{
					//	LineBoundingPos->bHasBoundNeg = false;
					//	LineBoundingPos->BoundNeg_TValue = FLT_MAX;
					//}
					//else if (LineBoundingPos->BoundPos == PointOfIntercept)
					//{
					//	LineBoundingPos->bHasBoundPos = false;
					//	LineBoundingPos->BoundPos_TValue = -FLT_MAX;
					//}
					BoundPos_TValue = NewT;
					BoundPos = PointOfIntercept;
					LineBoundingPos = Edge;
				}
			}
		}
		else{
			//Negitive Direction
			float NewT = Line->GetTValue(PointOfIntercept);
			if (bHasBoundNeg == false)
			{
				BoundNeg_TValue = NewT;
				BoundNeg = PointOfIntercept;
				LineBoundingNeg = Edge;
				bHasBoundNeg = true;
			}
			else
			{
				if (NewT < BoundNeg_TValue)
				{
					//CullOldData - Unsure If UseFull
					//if (LineBoundingNeg->BoundNeg == PointOfIntercept)
					//{
					//	LineBoundingNeg->bHasBoundNeg = false;
					//	LineBoundingNeg->BoundNeg_TValue = FLT_MAX;
					//}
					//else if (LineBoundingNeg->BoundPos == PointOfIntercept)
					//{
					//	LineBoundingNeg->bHasBoundPos = false;
					//	LineBoundingNeg->BoundPos_TValue = -FLT_MAX;
					//}


					BoundNeg_TValue = NewT;
					BoundNeg = PointOfIntercept;
					LineBoundingNeg = Edge;
				}
			}			
		}

		if (bHasBoundNeg && bHasBoundPos)
		{
			if (BoundNeg_TValue < BoundPos_TValue)
			{
				IsValidEdge = false;
			}
		}
	}
};

void VORO_CELL_EDGE::Draw(vec3 Col)
{
	if (bHasBoundPos && bHasBoundNeg && IsValidEdge)
	{
		//float IdealDistance = 5;
		//float DistanceToPos = pow(IdealDistance/glm::length(BoundPos - Cam),7);
		//float DistanceToNeg = pow(IdealDistance/glm::length(BoundNeg - Cam),7);
		//DistanceToPos = 1;
		//DistanceToNeg = 1;

		Gizmos::addLine(BoundPos, BoundNeg, glm::vec4(Col, 1), glm::vec4(Col, 1));
		//Gizmos::addTransform(glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(BoundPos, 1)));
		//Gizmos::addTransform(glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(BoundNeg, 1)));
	}
}

VORO_CELL_EDGE::VORO_CELL_EDGE(VORO_CELL_FACE* Face_This, VORO_CELL_FACE* Face_Other)
{
	FormingFace_Other = Face_Other;
	FormingFace_This = Face_This;
	Line3D LineDef = Face_This->Face->GetIntercept(Plane3D(Face_Other->Face->PointOnPlane, Face_Other->Face->Normal)); 
	Line = new Line3D(LineDef.PointOnLine, LineDef.Direction);
	bHasBoundPos = false;
	bHasBoundNeg = false;
	LineBoundingPos = nullptr;
	LineBoundingNeg = nullptr;
	BoundNeg_TValue = FLT_MAX;
	BoundPos_TValue = -FLT_MAX;
	IsValidEdge = true;
	if (Line->Direction == vec3(0))
		IsValidEdge = false;
};
VORO_CELL_EDGE::~VORO_CELL_EDGE()
{
	delete Line;
};


//VORO_CONTAINER
//=======================================================================

VORO_CONTAINER::VORO_CONTAINER(vec3 Min, vec3 Max, vec3 Div)
{
	MinXYZ = Min;
	MaxXYZ = Max;
	Center = (Min + Max) / 2.0f;

	D_MIN_C = (Min - Center);
	D_MAX_C = (Max - Center);


	DD_MIN_PC = D_MIN_C*2.0f;
	DD_MAX_PC = D_MAX_C*2.0f;

	Span = Max - Min;
	SubDivisionsXYZ = vec3(floor(Div.x),floor(Div.y),floor(Div.z));
	DivSizes = Span / SubDivisionsXYZ;


	DivArraySize = SubDivisionsXYZ.x*SubDivisionsXYZ.y*SubDivisionsXYZ.z;

	for (int Cell = 0; Cell < DivArraySize; ++Cell)
	{
		DivCell STD;
		Cells.push_back(STD);
	}


	FaceGenSeeds[6] = new VORO_SEED(Center, 1);
	FaceGenSeeds[0] = new VORO_SEED(vec3(DD_MAX_PC.x, 0, 0)+Center, 1);
	FaceGenSeeds[1] = new VORO_SEED(vec3(0, DD_MAX_PC.y, 0)+Center, 1);
	FaceGenSeeds[2] = new VORO_SEED(vec3(0, 0, DD_MAX_PC.z)+Center, 1);
	FaceGenSeeds[3] = new VORO_SEED(vec3(DD_MIN_PC.x, 0, 0)+Center, 1);
	FaceGenSeeds[4] = new VORO_SEED(vec3(0, DD_MIN_PC.y, 0)+Center, 1);
	FaceGenSeeds[5] = new VORO_SEED(vec3(0, 0, DD_MIN_PC.z)+Center, 1);


}
VORO_CONTAINER::~VORO_CONTAINER()
{
	//Remove All From Array
	for (int Cell = 0; Cell < Cells.size(); ++Cell)
	{
		for (int VORO_CELL = 0; VORO_CELL < Cells[Cell].size(); ++VORO_CELL)
		{
			delete Cells[Cell][VORO_CELL];
		}
	}

	//RemoveFaceGenSeeds
	delete FaceGenSeeds[6];
	delete FaceGenSeeds[0];
	delete FaceGenSeeds[1];
	delete FaceGenSeeds[2];
	delete FaceGenSeeds[3];
	delete FaceGenSeeds[4];
	delete FaceGenSeeds[5];


}

void VORO_CONTAINER::FillWithCubes()
{ 
	//FillWith Uncalculated VoroCells
	for (int X = 0; X < SubDivisionsXYZ.x; ++X)
	{
		for (int Y = 0; Y < SubDivisionsXYZ.y; ++Y)
		{
			for (int Z = 0; Z < SubDivisionsXYZ.z; ++Z)
			{
				int ID = CellID(X, Y, Z);
				Cells[ID].push_back(new VORO_CELL(D_MIN_C + vec3(X*DivSizes.x + DivSizes.x / 2, Y*DivSizes.y + DivSizes.y / 2, Z*DivSizes.z + DivSizes.z / 2), 1,VoroType::eVOID));
			}
		}
	}

	for (int X = 0; X < SubDivisionsXYZ.x; ++X)
	{
		for (int Y = 0; Y < SubDivisionsXYZ.y; ++Y)
		{
			for (int Z = 0; Z < SubDivisionsXYZ.z; ++Z)
			{
				int ID = CellID(X, Y, Z);

				//X-1
				if (!EdgeCont_X(X-1)) //Exists
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X - 1, Y, Z)][0]);
				else //Create
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3((X - 1)*DivSizes.x + DivSizes.x / 2, Y*DivSizes.y + DivSizes.y / 2, Z*DivSizes.z + DivSizes.z / 2), 1));

				//X+1
				if (!EdgeCont_X(X + 1))
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X + 1, Y, Z)][0]);
				else
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3((X + 1)* DivSizes.x + DivSizes.x / 2, Y*DivSizes.y + DivSizes.y / 2, Z*DivSizes.z + DivSizes.z / 2), 1));

				//Y-1
				if (!EdgeCont_Y(Y - 1))
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X, Y - 1, Z)][0]);
				else
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3(X* DivSizes.x + DivSizes.x / 2, (Y - 1)*DivSizes.y + DivSizes.y / 2, Z*DivSizes.z + DivSizes.z / 2), 1));

				//Y+1
				if (!EdgeCont_Y(Y + 1))
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X, Y + 1, Z)][0]);
				else
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3(X* DivSizes.x + DivSizes.x / 2, (Y + 1)*DivSizes.y + DivSizes.y / 2, Z*DivSizes.z + DivSizes.z / 2), 1));

				//Z-1
				if (!EdgeCont_Z(Z - 1))
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X, Y, Z - 1)][0]);
				else
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3(X* DivSizes.x + DivSizes.x / 2, Y*DivSizes.y + DivSizes.y / 2, (Z - 1)*DivSizes.z + DivSizes.z / 2), 1));

				//Z+1
				if (!EdgeCont_Z(Z + 1))
					Cells[ID][0]->AddSeedOLD(Cells[CellID(X, Y, Z + 1)][0]);
				else
					Cells[ID][0]->AddSeedOLD(&VORO_SEED(D_MIN_C + vec3(X* DivSizes.x + DivSizes.x / 2, Y*DivSizes.y + DivSizes.y / 2, (Z + 1)*DivSizes.z + DivSizes.z / 2), 1));

				Cells[ID][0]->CalculateFromFaces();
				Cells[ID][0]->BoundingRadius = DivSizesBoundingRadius;
				Cells[ID][0]->Gen_GL_Buffers();
			}
		}
	}
}

void VORO_CONTAINER::AddSeed(VORO_SEED* SEED)
{
	int ID = CellID(SEED->Location);
	vec3 MinDelta = MinXYZ - SEED->Location;
	vec3 MaxDelta = MaxXYZ - SEED->Location;

	bool Valid = false;
	if (MinDelta.x < 0 && MinDelta.y < 0 && MinDelta.z < 0) //Greater Then Mins
		if (MaxDelta.x > 0 && MaxDelta.y > 0 && MaxDelta.z > 0) // Less Then Maxs
			Valid = true;

	if (Valid)
	{
		Cells[ID].push_back(new VORO_CELL(SEED->Location,SEED->Scale,SEED->Type));
		if (LargestCellScale < SEED->Scale)
			LargestCellScale = SEED->Scale;
	}

}

int VORO_CONTAINER::CellID(vec3 Loc)
{
	return CellX(Loc.x) + CellY(Loc.y)*SubDivisionsXYZ.x + CellZ(Loc.z)*SubDivisionsXYZ.y*SubDivisionsXYZ.x;
}

int VORO_CONTAINER::CellID(int X, int Y, int Z)
{
	return X + Y*SubDivisionsXYZ.x + Z*SubDivisionsXYZ.y*SubDivisionsXYZ.x;
}

int VORO_CONTAINER::CellX(float LocX)
{
	return floor(LocX / DivSizes.x);
}
int VORO_CONTAINER::CellY(float LocY)
{
	return floor(LocY / DivSizes.y);
}
int VORO_CONTAINER::CellZ(float LocZ)
{
	return floor(LocZ / DivSizes.z);
}

bool VORO_CONTAINER::EdgeCont_X(int X)
{
	if (X <= 0)
		return true;
	else if (X >= SubDivisionsXYZ.x-1)
		return true;
	else
		return false;
}
bool VORO_CONTAINER::EdgeCont_Y(int Y)
{
	if (Y <= 0)
		return true;
	else if (Y >= SubDivisionsXYZ.y-1)
		return true;
	else
		return false;
}
bool VORO_CONTAINER::EdgeCont_Z(int Z)
{
	if (Z <= 0)
		return true;
	else if (Z >= SubDivisionsXYZ.z-1)
		return true;
	else
		return false;
}

VORO_CELL_CALCULATOR* VORO_CONTAINER::GenNewFromPoint(VORO_SEED* SEED)
{
	VORO_CELL_CALCULATOR* Result = new VORO_CELL_CALCULATOR(FaceGenSeeds[6]->Location, FaceGenSeeds[6]->Scale, FaceGenSeeds[6]->Type);
	for (int Seed = 0; Seed < 6; ++Seed)
	{
		Result->AddSeedOLD(FaceGenSeeds[Seed]);
	}
	Result->CalculateFromFaces();
	Result->Location = SEED->Location;
	Result->Scale = SEED->Scale;
	Result->Type = SEED->Type;
	//Result->GenBoundingRadius(); //Must be done after SEED is set && Faces are Calculated.
	return Result;

}
