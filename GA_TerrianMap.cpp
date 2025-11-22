#include "GA_TerrianMap.h"
#include "glm\ext.hpp"
#define _USE_MATH_DEFINES
#include <math.h>


MapTile::MapTile()
{
	Amp = FLT_MAX;
	Freq = FLT_MAX;
}

bool MapTile::NeedsNewData(float Frequency, float Amplitude, int X, int Y)
{
	if (Freq != Frequency || Amp != Amplitude || X != Xloc || Y != Yloc)
		return true;
	return false;
}

glm::vec3 GetTangent(glm::vec3 Normal)
{
	vec3 Tangent = vec3(1, 0, 0);
	if (abs(Normal.y) != 1)
		Tangent = glm::cross(glm::cross(Normal, vec3(0, 1, 0)), Normal);

	return Tangent;
}

void MapTile::GenData(float Frequency, float Amplitude, int X, int Y)
{
	if (NeedsNewData(Frequency, Amplitude, X, Y))
	{
		Xloc = X;
		Yloc = Y;
		Freq = Frequency;
		Amp = Amplitude;

		Heights[0] = GetPerlin(Frequency, Amplitude, X, Y);
		Heights[1] = GetPerlin(Frequency, Amplitude, X + 1, Y);
		Heights[2] = GetPerlin(Frequency, Amplitude, X, Y + 1);
		Heights[3] = GetPerlin(Frequency, Amplitude, X + 1, Y + 1);

		Normals[0] = GetNormal(Frequency, Amplitude, X, Y);
		Normals[1] = GetNormal(Frequency, Amplitude, X + 1, Y);
		Normals[2] = GetNormal(Frequency, Amplitude, X, Y + 1);
		Normals[3] = GetNormal(Frequency, Amplitude, X + 1, Y + 1);

		Tangents[0] = GetTangent(Normals[0]);
		Tangents[1] = GetTangent(Normals[1]);
		Tangents[2] = GetTangent(Normals[2]);
		Tangents[3] = GetTangent(Normals[3]);

	}

}



float MapTile::GetPerlin(float Frequency, float Amplitude, int X, int Y)
{
	return glm::perlin(glm::vec2((float)X, (float)Y) / Frequency)*Amplitude + glm::perlin(glm::vec2((float)X, (float)Y) / 0.2f*Frequency)*0.2f*Amplitude;
}

glm::vec3 MapTile::GetNormal(float Frequency, float Amplitude, int X, int Y)
{
	float HeightField[5];
	HeightField[0] = GetPerlin(Frequency,Amplitude,X,Y);
	HeightField[1] = GetPerlin(Frequency, Amplitude, X-1, Y);
	HeightField[2] = GetPerlin(Frequency, Amplitude, X+1, Y);
	HeightField[3] = GetPerlin(Frequency, Amplitude, X, Y-1);
	HeightField[4] = GetPerlin(Frequency, Amplitude, X, Y+1);

	float Deltas[4];

	Deltas[0] = HeightField[0] - HeightField[1]; //gradiant in x-
	Deltas[1] = HeightField[2] - HeightField[0]; //gradiant in x+
	Deltas[2] = HeightField[0] - HeightField[3]; //gradiant in y-
	Deltas[3] = HeightField[4] - HeightField[0]; //gradiant in y+

	glm::vec2 DeltaXY = glm::vec2(Deltas[0] + Deltas[1], Deltas[2] + Deltas[3])*0.5f;

	float AngleX = M_PI/2 - atan(DeltaXY.x);
	glm::vec3 NormalX = glm::vec3(cos(AngleX), sin(AngleX), 0);

	float AngleZ = M_PI / 2 - atan(DeltaXY.y);
	glm::vec3 NormalZ = glm::vec3(0, sin(AngleZ), cos(AngleZ));

	glm::vec3 Result = glm::vec3(NormalX + NormalZ)/2;
	return glm::normalize(Result);
	

}


GA_TerrianMap::GA_TerrianMap(glm::vec3 CameraLoc)
{
	Updated = false;

	Map = new Expandable3D_deque<MapTile>((int)CameraLoc.x, 0, (int)CameraLoc.z);
	Map->Add_MaxX();
	Map->Add_MaxY();
	Map->Add_MaxZ();

	HasGLBuffers = false;
}


GA_TerrianMap::~GA_TerrianMap()
{
}

void GA_TerrianMap::Draw(GL_Manager* Manager)
{
	GL_Manager::DrawTempCustomGeometry(GLBufferID);
}

bool GA_TerrianMap::UpdateMapSpace(glm::vec3 CameraLoc, float Freq, float Amp)
{
	if (UpdateRequired(CameraLoc) || Freq !=LastFreq || Amp != LastAmp)
	{
		LastFreq = Freq;
		LastAmp = Amp;
		int Minx = (int)CameraLoc.x - VissibleRange;
		int Maxx = (int)CameraLoc.x + VissibleRange;
		int Minz = (int)CameraLoc.z - VissibleRange;
		int Maxz = (int)CameraLoc.z + VissibleRange;

		if (Map->GetMinX() < Minx - VissibleRange*AdjustmentLeniancy)
		{
			while (Map->GetMinX() < Minx)
			{
				Map->Sub_MinX();
			}
		}
		if (Map->GetMaxX() > Maxx + VissibleRange*AdjustmentLeniancy)
		{
			while (Map->GetMaxX() > Maxx)
			{
				Map->Sub_MaxX();
			}
		}
		if (Map->GetMinZ() < Minz - VissibleRange*AdjustmentLeniancy)
		{
			while (Map->GetMinZ() < Minz)
			{
				Map->Sub_MinZ();
			}
		}
		if (Map->GetMaxZ() > Maxz +VissibleRange*AdjustmentLeniancy)
		{
			while (Map->GetMaxZ() > Maxz)
			{
				Map->Sub_MaxZ();
			}
		}





		while (Map->GetMinX() > Minx)
		{
			Map->Add_MinX();
		}
		while (Map->GetMaxX() < Maxx)
		{
			Map->Add_MaxX();
		}
		while (Map->GetMinZ() > Minz)
		{
			Map->Add_MinZ();
		}
		while (Map->GetMaxZ() < Maxz)
		{
			Map->Add_MaxZ();
		}

		for (int x = Minx; x < Maxx; ++x)
		{
			for (int z = Minz; z < Maxz; ++z)
			{
				MapTile* Tile = Map->GetAt(x, 0, z);
				Tile->GenData(Freq,Amp,x,z);
			}
		}

		DeleteGL_Buffers();
		CreateGL_Buffers();



		return true;
	}
	return false;
}

bool GA_TerrianMap::UpdateRequired(glm::vec3 CameraLoc)
{
	if (!AutoUpdate && !Updated)
	{
		Updated = true;
		return true;
	}
	else if (!AutoUpdate && Updated)
	{
		return false;
	}

	int Minx = (int)CameraLoc.x - VissibleRange;
	int Maxx = (int)CameraLoc.x + VissibleRange;
	int Minz = (int)CameraLoc.z - VissibleRange;
	int Maxz = (int)CameraLoc.z + VissibleRange;

	if (abs(Minx - Map->GetMinX()) > UpdateDeltaRequirment)
		return true;
	if (abs(Maxx - Map->GetMaxX()) > UpdateDeltaRequirment)
		return true;
	if (abs(Minz - Map->GetMinZ()) > UpdateDeltaRequirment)
		return true;
	if (abs(Maxz - Map->GetMaxZ()) > UpdateDeltaRequirment)
		return true;

	return false;

}

void GA_TerrianMap::CreateGL_Buffers()
{
	if (!HasGLBuffers)
	{
		vector<VertexComplex> Verts;
		vector<unsigned int> Indexs;
		int MinX = Map->GetMinX();
		int MaxX = Map->GetMaxX();
		int MinZ = Map->GetMinZ();
		int MaxZ = Map->GetMaxZ();
		int Y = 0;
		int VertsCount = 0;
		float TextureScale = 3;
		for (int X = MinX; X < MaxX; ++X)
		{
			for (int Z = MinZ; Z < MaxZ; ++Z)
			{
				MapTile* Tile = Map->GetAtQuick(X, Y, Z);

				Verts.push_back({ Tile->Xloc, Tile->Heights[0], Tile->Yloc, 1, Tile->Normals[0].x, Tile->Normals[0].y, Tile->Normals[0].z, 0, Tile->Tangents[0].x, Tile->Tangents[0].y, Tile->Tangents[0].z, 0, (Tile->Xloc) / TextureScale, (Tile->Yloc) / TextureScale });
				Verts.push_back({ Tile->Xloc+1, Tile->Heights[1], Tile->Yloc, 1, Tile->Normals[1].x, Tile->Normals[1].y, Tile->Normals[1].z, 0, Tile->Tangents[1].x, Tile->Tangents[1].y, Tile->Tangents[1].z, 0, (Tile->Xloc+1) / TextureScale, (Tile->Yloc) / TextureScale });
				Verts.push_back({ Tile->Xloc, Tile->Heights[2], Tile->Yloc+1, 1, Tile->Normals[2].x, Tile->Normals[2].y, Tile->Normals[2].z, 0, Tile->Tangents[2].x, Tile->Tangents[2].y, Tile->Tangents[2].z, 0, (Tile->Xloc) / TextureScale, (Tile->Yloc+1) / TextureScale });
				Verts.push_back({ Tile->Xloc+1, Tile->Heights[3], Tile->Yloc+1, 1, Tile->Normals[3].x, Tile->Normals[3].y, Tile->Normals[3].z, 0, Tile->Tangents[3].x, Tile->Tangents[3].y, Tile->Tangents[3].z, 0, (Tile->Xloc+1) / TextureScale, (Tile->Yloc+1) / TextureScale });

				Indexs.push_back(0+VertsCount);
				Indexs.push_back(1+VertsCount);
				Indexs.push_back(2+VertsCount);
				Indexs.push_back(1+VertsCount);
				Indexs.push_back(2+VertsCount);
				Indexs.push_back(3+VertsCount);

				VertsCount += 4;
			}
		}


		GLBufferID = GL_Manager::TemporaryCustomGeometry(Verts,Indexs);
		HasGLBuffers = true;
	}
}
void GA_TerrianMap::DeleteGL_Buffers()
{
	if (HasGLBuffers)
		GL_Manager::DeleteTempGeometry(GLBufferID);
	HasGLBuffers = false;
}