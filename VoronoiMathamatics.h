#pragma once
#include "aie\Gizmos.h"
#include "glm\glm.hpp"
#include <vector>

#include "Mathamatics3D.h"


class VoronoiCell;
class VoronoiFace;

using glm::vec3;
using glm::vec4;


enum VoroType
{
	//ContainerDefinitions
	eEmpty = -2,
	eMixed = -1,

	//CellTypes
	eVOID,
	eSEMIVOID,
	eSOLID,

	//TotalNumber CellTypes
	eTypes,
};


class VoronoiEdge
{
public:
	VoronoiEdge(Plane_3D Plane, Plane_3D Other);
	VoronoiEdge(int PlaneID, int OtherID, Plane_3D Plane, Plane_3D Other);

	Line_3D Line;
	glm::vec3 UpperPoint;
	glm::vec3 LowerPoint;
	float UpperT;
	float LowerT;
	bool Valid;

	int ThisPlaneID;
	int OtherPlaneID;

	void Draw(glm::vec4 Col);
	bool CheckPlane(Plane_3D Plane); //returns true if still valid
};

class VoronoiFace
{
public:
	VoronoiFace(vec3 Point, vec3 Normal);
	VoronoiFace(vec3 Point, vec3 Normal,int PlaneID);
	Plane_3D Plane;
	int PlaneID;
	std::vector<VoronoiEdge> Edges;
	bool HasHad3Edges; //Don't Cull Earlier then needed?

	VoronoiCell* FormingOther;
};

class VoronoiSeed
{
public:
	VoronoiSeed(vec3 _loc, float Scale);
	vec3 Location;
	float Scale;
	VoroType Type;
};

class VoronoiCell:public VoronoiSeed
{
public:
	VoronoiCell(vec3 Location, float Scale);
	VoronoiCell();
	~VoronoiCell();
	void GenFaceFromSeed(VoronoiSeed* Seed); //AutoCalcuations
	void GenFaceFromSeed(VoronoiSeed Seed); //AutoCalcuations
	void GenFaceFromData(vec3 Pos,vec3 Normal,VoronoiSeed* SeedRef); //AutoCalcuations
	void RecalculateExistingFromData(vec3 Pos, vec3 Normal, VoronoiSeed* SeedRef); //AutoCalcuations no new Face Generated
	void CleanFaces();
	void CopyFaceEdges(); //Ensures any missing Edges are filled

	void AddBoundingBox(vec3 Min, vec3 Max,bool GensFaces); //if gens faces is true, the limits will have faces, else, the limits will not generate faces

	float BoundingRadius;
	void GenBoundingRadius();

	void Gen_GL_Buffers();
	void Delete_GL_Buffers();


	void Draw();
	void DrawEdges(glm::vec4 Col);
	std::vector<VoronoiFace> Faces;
private:
	bool HasGLBuffers;
	void RecalculateOldFace(int Face, int PlaneRef);
	void CaclulateNewFace();
	std::vector<Plane_3D> Planes;


	//OpenGl Data
	unsigned int GenBuffer_FaceCount;
	unsigned int* GenBuffer_EdgeCount;

	//OGL Buffer Data; - One for Each Face;
	unsigned int* VBO;
	unsigned int* VAO;
	unsigned int* IBO;



};