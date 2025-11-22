#include "VoronoiMathamatics.h"
#include "aie\Gizmos.h"
#include "Vertex.h"
#include <loadgen/gl_core_4_4.h>
//VoronoiEdge::VoronoiEdge(Plane_3D Plane, VoronoiFace Other)
//{
//	Line = Plane.GetInterceptLine(Other.Plane);
//	FaceOtherRef = Other.FaceRef;
//	UpperT = FLT_MAX;
//	LowerT = -FLT_MAX;
//	UpperPoint = Line.PointOnLine + Line.Direction*UpperT;
//	LowerPoint = Line.PointOnLine + Line.Direction*LowerT;
//
//	if (Line.Direction != glm::vec3(0))
//		Valid = true;
//	else
//		Valid = false;
//}

VoronoiEdge::VoronoiEdge(Plane_3D Plane, Plane_3D Other)
{
	Line = Plane.GetInterceptLine(Other);
	UpperT = FLT_MAX;
	LowerT = -FLT_MAX;
	UpperPoint = Line.PointOnLine + Line.Direction*UpperT;
	LowerPoint = Line.PointOnLine + Line.Direction*LowerT;

	if (Line.Direction != glm::vec3(0))
		Valid = true;
	else
		Valid = false;
}

VoronoiEdge::VoronoiEdge(int PlaneID, int OtherID, Plane_3D Plane, Plane_3D Other)
{
	OtherPlaneID = OtherID;
	ThisPlaneID = PlaneID;
	Line = Plane.GetInterceptLine(Other);
	UpperT = FLT_MAX;
	LowerT = -FLT_MAX;
	UpperPoint = Line.PointOnLine + Line.Direction*UpperT;
	LowerPoint = Line.PointOnLine + Line.Direction*LowerT;

	if (Line.Direction != glm::vec3(0))
		Valid = true;
	else
		Valid = false;
}

void VoronoiEdge::Draw(glm::vec4 Col)
{
	Gizmos::addLine(LowerPoint, UpperPoint, Col);
}

//void VoronoiEdge::FaceRemoved(int FaceRef)
//{
//	if (FaceOtherRef > FaceRef)
//		--FaceOtherRef;
//}

bool VoronoiEdge::CheckPlane(Plane_3D Plane)
{
	float Dot = glm::dot(Plane.Normal, Line.Direction);
	if (Dot > 0)
	{
		float T = Plane.GetIntercept(Line);
		if (T < UpperT)
		{
			UpperT = T;
			UpperPoint = Line.PointOnLine + Line.Direction*UpperT;
		}
	}
	if (Dot < 0)
	{
		float T = Plane.GetIntercept(Line);
		if (T > LowerT)
		{
			LowerT = T;
			LowerPoint = Line.PointOnLine + Line.Direction*LowerT;
		}
	}
	if (Dot == 0)
	{
		//if (!Plane.IsPointUnder(Line.PointOnLine))
		//	Valid = false;
	}

	if (LowerT >= UpperT)
		Valid = false;

	return Valid;
}


//VoronoiFace::VoronoiFace(vec3 Point, vec3 Normal, int Number)
//{
//	Plane = Plane_3D(Point, Normal);
//	FaceRef = Number;
//}
VoronoiFace::VoronoiFace(vec3 Point, vec3 Normal)
{
	Plane = Plane_3D(Point, Normal);
	FormingOther = nullptr;
}

VoronoiFace::VoronoiFace(vec3 Point, vec3 Normal, int _PlaneID)
{
	Plane = Plane_3D(Point, Normal);
	PlaneID = _PlaneID;
}

//void VoronoiFace::FaceRemoved(int _FaceRef)
//{
//	if (FaceRef > _FaceRef)
//		--FaceRef;
//
//	for (int Edge = 0; Edge < Edges.size(); ++Edge)
//	{
//		Edges[Edge].FaceRemoved(_FaceRef);
//	}
//}

VoronoiSeed::VoronoiSeed(vec3 _loc, float _Scale)
{
	Location = _loc;
	Scale = _Scale;
	Type = VoroType::eVOID;
}

VoronoiCell::VoronoiCell(vec3 _Location, float _Scale) :VoronoiSeed(_Location,_Scale)
{
	BoundingRadius = FLT_MAX;
}

VoronoiCell::VoronoiCell() : VoronoiSeed(vec3(0), 1)
{
	BoundingRadius = FLT_MAX;
}

VoronoiCell::~VoronoiCell()
{
	if (HasGLBuffers)
		Delete_GL_Buffers();
}

//void VoronoiCell::RemoveFace(int FaceRef)
//{
//	Faces.erase(Faces.begin() + FaceRef);
//
//	for (int Face = 0; Face < Faces.size(); ++Face)
//	{
//		Faces[Face].FaceRemoved(FaceRef);
//	}
//}

void VoronoiCell::GenBoundingRadius()
{
	float Max = -1;
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
		{
			vec3 Delta = Faces[Face].Edges[Edge].LowerPoint - Location;
			if (glm::length(Delta) > Max)
				Max = glm::length(Delta);
		}
	}
	if (Max == -1)
		BoundingRadius = FLT_MAX;
	else
		BoundingRadius = Max;

}

void VoronoiCell::GenFaceFromSeed(VoronoiSeed Seed)
{
	float Ratio = Seed.Scale*(1 / Scale);
	float LocationRatio = 1 / (1 + Ratio);
	vec3 Direction = Seed.Location - Location;
	vec3 Loc = Location + Direction*LocationRatio;
	if (glm::length(Direction) < BoundingRadius)
	{
		GenFaceFromData(Loc, Direction,nullptr);
	}
}

void VoronoiCell::GenFaceFromSeed(VoronoiSeed* Seed)
{
	float Ratio = Seed->Scale*(1 / Scale);
	float LocationRatio = 1 / (1 + Ratio);
	vec3 Direction = Seed->Location - Location;
	vec3 Loc = Location + Direction*LocationRatio;
	//if (glm::length(Direction) < BoundingRadius)
	//{
		GenFaceFromData(Loc, Direction, Seed);
	//}
}

void VoronoiCell::GenFaceFromData(vec3 Pos, vec3 Normal,VoronoiSeed* SeedRef)
{
	Plane_3D NewPlane = Plane_3D(Pos, Normal);
	Line_3D NormalLine = Line_3D(Location, Normal);
	float ProperPointT = NewPlane.GetIntercept(NormalLine);
	Plane_3D FinalPlane = Plane_3D(NormalLine.GetPointFromT(ProperPointT), Normal);
	if (abs(ProperPointT) < BoundingRadius)
	{
		int FaceLoc = Faces.size();
		int PlaneLoc = Planes.size();
		VoronoiFace NewFace = VoronoiFace(Pos, Normal, PlaneLoc);
		NewFace.FormingOther = (VoronoiCell*)SeedRef;
		Faces.push_back(NewFace);
		Planes.push_back(FinalPlane);


		CaclulateNewFace();
		for (int Face = 0; Face <FaceLoc; ++Face)
		{
			RecalculateOldFace(Face,PlaneLoc);
		}
		for (int Face = 1; Face < Faces.size(); ++Face) //the first face gets culled unnecisarily since it has no forming edge to start with, and sometimes later.
		{
			if (Faces[Face].Edges.size() <= 0)
			{
				Faces.erase(Faces.begin() + Face);
					//RemoveFace(Face);
				--Face;
			}
		}


		GenBoundingRadius();
	}
}

void VoronoiCell::RecalculateExistingFromData(vec3 Pos, vec3 Normal, VoronoiSeed* SeedRef)
{
	Plane_3D NewPlane = Plane_3D(Pos, Normal);
	Line_3D NormalLine = Line_3D(Location, Normal);
	float ProperPointT = NewPlane.GetIntercept(NormalLine);
	Plane_3D FinalPlane = Plane_3D(NormalLine.GetPointFromT(ProperPointT), Normal);
	if (abs(ProperPointT) < BoundingRadius)
	{
		int FaceLoc = Faces.size();
		int PlaneLoc = Planes.size();
		Planes.push_back(FinalPlane);
		for (int Face = 0; Face <FaceLoc; ++Face)
		{
			RecalculateOldFace(Face, PlaneLoc);
		}
		for (int Face = 0; Face < Faces.size(); ++Face)
		{
			if (Faces[Face].Edges.size() <= 0)
			{
				Faces.erase(Faces.begin() + Face);
				//RemoveFace(Face);
				--Face;
			}
		}

		GenBoundingRadius();
	}
}


void VoronoiCell::AddBoundingBox(vec3 Min, vec3 Max, bool GensFaces)
{
	if (GensFaces)
	{
		//GenFaceFromSeed(VoronoiSeed(vec3(2*Min.x - Location.x, Location.y, Location.z), 1));
		//GenFaceFromSeed(VoronoiSeed(vec3(2*Max.x - Location.x, Location.y, Location.z), 1));
		//GenFaceFromSeed(VoronoiSeed(vec3(Location.x, 2 * Min.y - Location.y, Location.z), 1));
		//GenFaceFromSeed(VoronoiSeed(vec3(Location.x, 2 * Max.y - Location.y, Location.z), 1));
		//GenFaceFromSeed(VoronoiSeed(vec3(Location.x, Location.y, 2 * Min.z - Location.z), 1));
		//GenFaceFromSeed(VoronoiSeed(vec3(Location.x, Location.y, 2 * Max.z - Location.z), 1));
		GenFaceFromData(vec3(Min.x, Location.y, Location.z), vec3(-1, 0, 0), nullptr);
		GenFaceFromData(vec3(Max.x, Location.y, Location.z), vec3(1, 0, 0), nullptr);
		GenFaceFromData(vec3(Location.x, Min.y, Location.z), vec3(0, -1, 0), nullptr);
		GenFaceFromData(vec3(Location.x, Max.y, Location.z), vec3(0, 1, 0), nullptr);
		GenFaceFromData(vec3(Location.x, Location.y, Min.z), vec3(0, 0, -1), nullptr);
		GenFaceFromData(vec3(Location.x, Location.y, Max.z), vec3(0, 0, 1), nullptr);
	}
	else
	{
		RecalculateExistingFromData(vec3(Min.x, Location.y, Location.z), vec3(-1, 0, 0), nullptr);
		RecalculateExistingFromData(vec3(Max.x, Location.y, Location.z), vec3(1, 0, 0), nullptr);
		RecalculateExistingFromData(vec3(Location.x, Min.y, Location.z), vec3(0, -1, 0), nullptr);
		RecalculateExistingFromData(vec3(Location.x, Max.y, Location.z), vec3(0, 1, 0), nullptr);
		RecalculateExistingFromData(vec3(Location.x, Location.y, Min.z), vec3(0, 0, -1), nullptr);
		RecalculateExistingFromData(vec3(Location.x, Location.y, Max.z), vec3(0, 0, 1), nullptr);
	}
}


void VoronoiCell::RecalculateOldFace(int Face, int PlaneRef)
{
	VoronoiEdge NewEdge = VoronoiEdge(Faces[Face].PlaneID, PlaneRef, Faces[Face].Plane, Planes[PlaneRef]);
	//for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
	//{
	//	Faces[Face].Edges[Edge].LowerT = -FLT_MAX;
	//	Faces[Face].Edges[Edge].UpperT = FLT_MAX;
	//	Faces[Face].Edges[Edge].Valid = true;
	//}
	for (int FaceRef = 0; FaceRef < Planes.size()-1; ++FaceRef) // Check EveryFace to form new edge
	{
		NewEdge.CheckPlane(Planes[FaceRef]);
	}

	for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge) //Let Every Edge Existing Consider The New Face
	{
		if (!Faces[Face].Edges[Edge].CheckPlane(Planes[PlaneRef]))
		{
			Faces[Face].Edges.erase(Faces[Face].Edges.begin() + Edge);
			--Edge;
		}
	}

	if (NewEdge.Valid)
		Faces[Face].Edges.push_back(NewEdge);
}

void VoronoiCell::CaclulateNewFace()
{
	int Face = Faces.size() - 1;
	int NewPlane = Planes.size() - 1;
	int NewFace = Faces.size() - 1;
	for (int Plane = 0; Plane < NewPlane; ++Plane)
	{
		VoronoiEdge NewEdge = VoronoiEdge(NewPlane,Plane,Planes[NewPlane], Planes[Plane]);
		Faces[NewFace].Edges.push_back(NewEdge);
	}
	for (int Edge = 0; Edge < Faces[NewFace].Edges.size(); ++Edge)
	{
		for (int Edge2 = 0; Edge2 < Faces[NewFace].Edges.size(); ++Edge2)
		{
			if (Edge != Edge2 && Faces[NewFace].Edges[Edge].OtherPlaneID != Edge2)
			{
				Faces[NewFace].Edges[Edge].CheckPlane(Planes[Edge2]);
			}
		}
	}
	for (int Edge = 0; Edge < Faces[NewFace].Edges.size(); ++Edge)
	{
		if (!Faces[NewFace].Edges[Edge].Valid)
		{
			Faces[NewFace].Edges.erase(Faces[NewFace].Edges.begin() + Edge);
			--Edge;
		}
	}
}

void VoronoiCell::CleanFaces()
{



	//for (int Face = 0; Face < Faces.size(); ++Face)
	//{
	//	vec3 Average;
	//	for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
	//	{
	//		Average += Faces[Face].Edges[Edge].UpperPoint;
	//		Average += Faces[Face].Edges[Edge].LowerPoint;
	//	}
	//	Average /= 2*Faces[Face].Edges.size();
	//	Faces[Face].Plane.PointOnPlane = Average;
	//}



	//for (int Face = 0; Face < Faces.size(); ++Face)
	//{
	//	for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
	//	{
	//		Faces[Face].Edges[Edge].LowerT = -FLT_MAX;
	//		Faces[Face].Edges[Edge].UpperT = FLT_MAX;
	//		Faces[Face].Edges[Edge].Valid = true;
	//	}
	//}
	//
	//
	//for (int Face = 0; Face < Faces.size(); ++Face)
	//{
	//	for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
	//	{
	//		for (int FaceRef = 0; FaceRef < Faces.size(); ++FaceRef)
	//		{
	//			if (Face != FaceRef)
	//			{
	//				if (!Faces[Face].Edges[Edge].CheckPlane(Faces[FaceRef].Plane))
	//				{
	//					Faces[Face].Edges.erase(Faces[Face].Edges.begin() + Edge);
	//					--Edge;
	//					FaceRef = Faces.size();
	//				}
	//			}
	//		}
	//	}
	//}
}

void VoronoiCell::CopyFaceEdges()
{
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		int ThisFacesPlaneID = Faces[Face].PlaneID;
		std::vector<VoronoiEdge> ReliventEdges;

		//Find Relivent Edges;
		for (int Face2 = 0; Face2 < Faces.size(); ++Face2) 
		{
			if (Face != Face2)
			{
				for (int Edge = 0; Edge < Faces[Face2].Edges.size(); ++Edge)
				{
					if (Faces[Face2].Edges[Edge].OtherPlaneID == ThisFacesPlaneID)
					{
						ReliventEdges.push_back(Faces[Face2].Edges[Edge]);
					}
				}
			}
		}

		//Check if Edges Already Exist
		for (int R_Edge = 0; R_Edge <ReliventEdges.size(); ++R_Edge)
		{
			bool AlreadyExists = false;
			for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
			{
				if (Faces[Face].Edges[Edge].Line.Direction == ReliventEdges[R_Edge].Line.Direction || Faces[Face].Edges[Edge].Line.Direction == -ReliventEdges[R_Edge].Line.Direction)
				{
					AlreadyExists = true;
					float Length = Faces[Face].Edges[Edge].UpperT - Faces[Face].Edges[Edge].LowerT;
					float LengthR = ReliventEdges[R_Edge].UpperT - ReliventEdges[R_Edge].LowerT;
					if (LengthR > Length)
					{
						if (Faces[Face].FormingOther != nullptr) //HackStatement
						{
							//Faces[Face].Edges[Edge].UpperPoint = ReliventEdges[R_Edge].UpperPoint;
							//Faces[Face].Edges[Edge].LowerPoint = ReliventEdges[R_Edge].LowerPoint;
							//Faces[Face].Edges[Edge].UpperT = ReliventEdges[R_Edge].UpperT;
							//Faces[Face].Edges[Edge].LowerT = ReliventEdges[R_Edge].LowerT;
							//Faces[Face].Edges[Edge].Line = ReliventEdges[R_Edge].Line;
							//ReliventEdges.erase(ReliventEdges.begin() + R_Edge);
							//--R_Edge;

							Faces[Face].Edges[Edge] = ReliventEdges[R_Edge];

							//Edge = Faces[Face].Edges.size();
							

						}
							//Faces[Face].Edges[Edge] = ReliventEdges[R_Edge];
					}
				}
				else if (Faces[Face].Edges[Edge].Line.Direction == -ReliventEdges[R_Edge].Line.Direction)
				{

				}
			}
			if (!AlreadyExists)
			{
				Faces[Face].Edges.push_back(ReliventEdges[R_Edge]);
			}
			//Faces[Face].Edges.push_back(ReliventEdges[R_Edge]);
		}
	}
}


//void VoronoiCell::AddFace(VoronoiSeed Seed)
//{
//	float Ratio = Seed.Scale*(1 / Scale);
//	float LocationRatio = 1 / (1 + Ratio);
//	vec3 Direction = Seed.Location - Location;
//	vec3 Loc = Location + Direction*LocationRatio;
//
//	int FaceLoc = Faces.size();
//	Faces.push_back(VoronoiFace(Loc, Direction, Faces.size()));
//}
//
//void VoronoiCell::AddFaceFromData(vec3 Pos, vec3 Normal)
//{
//	Faces.push_back(VoronoiFace(Pos, Normal, Faces.size()));
//}
//
//void VoronoiCell::GenFromFaces()
//{
//	for (int Face = 0; Face < Faces.size(); ++Face)
//	{
//		for (int Face2 = 0; Face2 < Faces.size(); ++Face2) //Face we are comparing with
//		{
//			if (Face != Face2)
//			{
//				VoronoiEdge New = VoronoiEdge(Faces[Face].Plane, Faces[Face2]);
//				if (New.Line.Direction != vec3(0))
//					Faces[Face].Edges.push_back(New);
//			}
//		}
//
//		//Edges
//		for (int Edge1 = 0; Edge1 < Faces[Face].Edges.size(); ++Edge1)
//		{
//			for (int Edge2 = 0; Edge2 < Faces[Face].Edges.size(); ++Edge2)
//			{
//				if (Edge1 != Edge2)
//				{
//					Faces[Face].Edges[Edge1].CheckPlane(Faces[Faces[Face].Edges[Edge2].FaceOtherRef].Plane);        /*CheckIntercept_wEdge(Faces[Face]->Edges[Edge2]);*/
//				}
//			}
//		}
//
//		for (int Edge1 = 0; Edge1 < Faces[Face].Edges.size(); ++Edge1)
//		{
//			if (!Faces[Face].Edges[Edge1].Valid)
//			{
//				Faces[Face].Edges.erase(Faces[Face].Edges.begin() + Edge1);
//				--Edge1;
//			}
//		}
//	}
//
//	//CullFaces
//	for (int Face = 0; Face < Faces.size(); ++Face)
//	{
//		if (Faces[Face].Edges.size() == 0)
//		{
//			RemoveFace(Face);
//			--Face;
//		}
//	}
//
//	////ReSizeVectors  = Memory Saving
//	//Faces.resize(Faces.size());
//	//for (int Face = 0; Face < Faces.size(); ++Face)
//	//{
//	//	Faces[Face].Edges.resize(Faces[Face].Edges.size());
//	//}
//
//	GenBoundingRadius();
//}

void VoronoiCell::DrawEdges(glm::vec4 Col)
{
	for (int Face = 0; Face < Faces.size(); ++Face)
	{
		//Face = 2;
		//Gizmos::addTransform(glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(Faces[Face].Plane.PointOnPlane, 1)));

		for (int Edge = 0; Edge < Faces[Face].Edges.size(); ++Edge)
		{
			Faces[Face].Edges[Edge].Draw(Col);
		}
		//Face = Faces.size();
	}
}

//void VoronoiCell::GenBoundingBox(float Size)
//{
//	AddFaceFromData(Location + vec3(Size / 2, 0, 0), vec3(1, 0, 0));
//	AddFaceFromData(Location + vec3(-Size / 2, 0, 0), vec3(-1, 0, 0));
//	AddFaceFromData(Location + vec3(0, Size / 2, 0), vec3(0, 1, 0));
//	AddFaceFromData(Location + vec3(0, -Size / 2, 0), vec3(0, -1, 0));
//	AddFaceFromData(Location + vec3(0, 0, Size / 2), vec3(0, 0, 1));
//	AddFaceFromData(Location + vec3(0, 0, -Size / 2), vec3(0,0, -1));
//	//AddFaceFromData(Location + vec3(0.5f, 1, 1.5f), vec3(0.5f, 1, 1.5f));
//	GenFromFaces();
//}

void::VoronoiCell::Draw()
{
	if (Type > VoroType::eVOID)
	{
		for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
		{
			glBindVertexArray(VAO[Face]);
			glDrawElements(GL_TRIANGLES, GenBuffer_EdgeCount[Face] * 3, GL_UNSIGNED_INT, nullptr);
		}
	}
}

void VoronoiCell::Delete_GL_Buffers()
{
	for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
	{
		glDeleteVertexArrays(1, &VAO[Face]);
		glDeleteBuffers(1, &VBO[Face]);
		glDeleteBuffers(1, &IBO[Face]);
	}
	delete[] VBO;
	delete[] VAO;
	delete[] IBO;
	delete[] GenBuffer_EdgeCount;

	HasGLBuffers = false;
}

void VoronoiCell::Gen_GL_Buffers()
{
	GenBuffer_FaceCount = Faces.size();
	VBO = new unsigned int[GenBuffer_FaceCount];
	VAO = new unsigned int[GenBuffer_FaceCount];
	IBO = new unsigned int[GenBuffer_FaceCount];
	GenBuffer_EdgeCount = new unsigned int[GenBuffer_FaceCount];
	float TexScale = 4;

	for (int Face = 0; Face < GenBuffer_FaceCount; ++Face)
	{
		GenBuffer_EdgeCount[Face] = Faces[Face].Edges.size();
		vec3 AveragePoint;
		vec3 Normal = Planes[Faces[Face].PlaneID].Normal;
		vec3 Tangent = vec3(1,0,0);
		if (abs(Normal.y) != 1)
			Tangent = glm::cross(glm::cross(Normal, vec3(0, 1, 0)),Normal);


		//Define AveragePoint;
		for (int Edge = 0; Edge < GenBuffer_EdgeCount[Face]; ++Edge)
		{
			AveragePoint += Faces[Face].Edges[Edge].UpperPoint;
			AveragePoint += Faces[Face].Edges[Edge].LowerPoint;
		}
		AveragePoint /= 2*(GenBuffer_EdgeCount[Face]);

		//GenAnArray;
		VertexComplex* Array;
		Array = new VertexComplex[GenBuffer_EdgeCount[Face] * 3];

		if (abs(glm::dot(Normal, vec3(0, 1, 0))) > 1 / pow(2, 0.5f))
		{
			//Tangent = vec3(1, 0, 0);
			for (int Edge = 0; Edge < GenBuffer_EdgeCount[Face]; ++Edge)
			{
				vec3 P = Faces[Face].Edges[Edge].UpperPoint;
				vec3 N = Faces[Face].Edges[Edge].LowerPoint;

				//PointA;
				Array[Edge * 3 + 0] = { P.x, P.y, P.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, P.x / TexScale, P.z / TexScale };
				//PointB;
				Array[Edge * 3 + 1] = { N.x, N.y, N.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, N.x / TexScale, N.z / TexScale };
				//PointC;
				Array[Edge * 3 + 2] = { AveragePoint.x, AveragePoint.y, AveragePoint.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, AveragePoint.x / TexScale, AveragePoint.z / TexScale };

			}
		}
		else
		{
			//Tangent = vec3(0, -1,0);
			for (int Edge = 0; Edge < GenBuffer_EdgeCount[Face]; ++Edge)
			{
				vec3 P = Faces[Face].Edges[Edge].UpperPoint;
				vec3 N = Faces[Face].Edges[Edge].LowerPoint;

				//PointA;
				Array[Edge * 3 + 0] = { P.x, P.y, P.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, (P.z + P.x) / TexScale, P.y / TexScale };
				//PointB;
				Array[Edge * 3 + 1] = { N.x, N.y, N.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, (N.z + N.x) / TexScale, N.y / TexScale };
				//PointC;
				Array[Edge * 3 + 2] = { AveragePoint.x, AveragePoint.y, AveragePoint.z, 1, Normal.x, Normal.y, Normal.z, 0, Tangent.x, Tangent.y, Tangent.z, 0, (AveragePoint.z + AveragePoint.x) / TexScale, AveragePoint.y / TexScale };

			}
		}

		unsigned int* IndexData;
		IndexData = new unsigned int[GenBuffer_EdgeCount[Face] * 3];
		for (int VertexID = 0; VertexID < GenBuffer_EdgeCount[Face] * 3; ++VertexID)
		{
			IndexData[VertexID] = VertexID;
		}

		glGenVertexArrays(1, &VAO[Face]);
		glBindVertexArray(VAO[Face]);

		glGenBuffers(1, &VBO[Face]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[Face]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(VertexComplex)*GenBuffer_EdgeCount[Face] * 3, Array, GL_STATIC_DRAW); //the 10 here seems to indicate the length of the strip of data in VertexBuffer, multiplied by 4 for the full length

		glGenBuffers(1, &IBO[Face]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO[Face]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*GenBuffer_EdgeCount[Face] * 3, IndexData, GL_STATIC_DRAW); //3 floats long, 2 floats deep indexData[]

		glEnableVertexAttribArray(0); // zero seems to indicate the location = 0 in the vertex shader, again that same number for is the first call in the following line
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), 0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 12);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 4);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexComplex), ((char*)0) + sizeof(float) * 8);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		//deleteArray;
		delete Array;
		delete IndexData;

		HasGLBuffers = true;
	}
}
