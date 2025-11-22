#pragma once
#include <vector>
#include "GL_Manager.h"
using namespace std;

enum fShape
{
	fSphere,
	fCylinder,
	fBox,
	fTorus,
};

enum fModSpace
{
	fTile1D,
	fTile2D,
	fTile3D,

	fLimitedTile1D,
	fLimitedTile2D,
	fLimitedTile3D,

	RotatePlane,
	GravityWell,

};

enum fMeld
{
	Add,
	Subtract,
};

struct fDist_Asset
{
	fShape Shape;
	vector<float> fShapeData;
	vector<int> iShapeData;
	int TextureID;
	string GenFormula();
};

class fDist_SpaceManager
{
public:
	std::vector<int> AccessInstructions;

	int AddTexturePackage(glm::vec2 UV_Scale, glm::vec2 UV_Offset, bool WorldSpaceRelitive, int MaterialID);

	int AddSphere(vec3 Loc, vec3 Rotation, float Radius, int TexturePackage);
	int AddBox(vec3 Loc, vec3 Rotation, vec3 Size, int TexturePackage);
	int AddCylinder(vec3 Loc, vec3 Rotation, float Radius, float Height, int TexturePackage);
	int AddTaperedCylinder(vec3 Loc, vec3 Rotation, float Rbot, float Rtop, int TexturePackage);
	int AddTorus(vec3 Loc, vec3 Rotation, float R1, float R2, int TexturePackage);

	fDist_SpaceManager();
	~fDist_SpaceManager();
};

