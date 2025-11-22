#include "GA_OldBuilding.h"
#include <minmax.h>


GA_OldBuilding::GA_OldBuilding()
{
}


GA_OldBuilding::~GA_OldBuilding()
{
	for (int j = 0; j < Boxs.size(); ++j)
	{
		delete Boxs[j];
	}
	
}

void GA_OldBuilding::ApplyCuts(std::vector<glm::vec4> CutSpheres)
{
	for (int Box = 0; Box < Boxs.size(); ++Box)
	{
		Boxs[Box]->SetInsideSphereToType(VoroType::eSOLID, glm::vec3(0), 100000);
		for (int Sphere = 0; Sphere < CutSpheres.size(); ++Sphere)
		{
			Boxs[Box]->SetInsideSphereToType(VoroType::eVOID, glm::vec3(CutSpheres[Sphere]), CutSpheres[Sphere].w);
		}
	}
}

void GA_OldBuilding::GenBuilding(int Xunits, int Zunits, float UnitSize, float Height, float RoofIndent, float RoofRowSpacing, float RoofRowSize, int DoorX, int DoorZ, float DoorMin, float DoorMax, float WallWidth,int Seeds, std::vector<glm::vec4> CutSpheres)
{
	vec3 Min = vec3(0);
	vec3 Max = vec3(Xunits*UnitSize, Height, Zunits*UnitSize);
	vec3 Dimensions = Max - Min;

	vec3 RoofMin = vec3(Min.x + WallWidth, Max.y - 2 * RoofIndent, Min.z + WallWidth);
	vec3 RoofMax = vec3(Max.x - WallWidth, Max.y - RoofIndent, Max.z - WallWidth);
	Boxs.push_back(new VoronoiBoxContainer(RoofMin, RoofMax));


	//XminWall
	vec3 Amin = Min;
	vec3 Amax = vec3(Min.x + WallWidth, Max.y, Max.z);
	if (DoorX*UnitSize != Min.x)
	{
		Boxs.push_back(new VoronoiBoxContainer(Amin, Amax));
	}
	else
	{
		if (DoorZ != Dimensions.z)
		{
			Boxs.push_back(new VoronoiBoxContainer(Amin, Amax - vec3(0, Dimensions.y - DoorMin, 0))); //Bottom
			Boxs.push_back(new VoronoiBoxContainer(Amin + vec3(0, DoorMax, 0), Amax)); //Top
			if (Amin.z != Amax.z - ((Amax.z - Amin.z) - DoorZ*UnitSize))
				Boxs.push_back(new VoronoiBoxContainer(Amin + vec3(0, DoorMin, 0), Amax - vec3(0, Dimensions.y - DoorMax, (Amax.z - Amin.z) - DoorZ*UnitSize))); //CenterA
			if (Amin.z + DoorZ*UnitSize + UnitSize != Amax.z)
				Boxs.push_back(new VoronoiBoxContainer(Amin + vec3(0, DoorMin, DoorZ*UnitSize + UnitSize), Amax - vec3(0, Dimensions.y - DoorMax, 0))); //CenterB
		}
		else
		{
			Boxs.push_back(new VoronoiBoxContainer(Amin, Amax));
		}

	}
	//XMaxWall
	vec3 Bmin = vec3(Max.x - WallWidth, Min.y, Min.z);
	vec3 Bmax = vec3(Max.x, Max.y, Max.z);
	if (DoorX*UnitSize != Max.x)
	{
		Boxs.push_back(new VoronoiBoxContainer(Bmin, Bmax));
	}
	else
	{
		Boxs.push_back(new VoronoiBoxContainer(Bmin, Bmax - vec3(0, Dimensions.y - DoorMin, 0))); //Bottom
		Boxs.push_back(new VoronoiBoxContainer(Bmin + vec3(0, DoorMax, 0), Bmax)); //Top
		if (Bmin.z != Bmax.z - ((Bmax.z - Bmin.z) - DoorZ*UnitSize))
			Boxs.push_back(new VoronoiBoxContainer(Bmin + vec3(0, DoorMin, 0), Bmax - vec3(0, Dimensions.y - DoorMax, (Bmax.z - Bmin.z) - DoorZ*UnitSize))); //CenterA
		if (Bmin.z + DoorZ*UnitSize + UnitSize != Bmax.z)
			Boxs.push_back(new VoronoiBoxContainer(Bmin + vec3(0, DoorMin, DoorZ*UnitSize + UnitSize), Bmax - vec3(0, Dimensions.y - DoorMax, 0))); //CenterB

	}
	//ZminWall
	vec3 Cmin = Min;
	vec3 Cmax = vec3(Max.x, Max.y, Min.z + WallWidth);
	vec3 Dmin = vec3(Min.x, Min.y, Max.z - WallWidth);
	vec3 Dmax = vec3(Max.x, Max.y, Max.z);
	if (DoorZ*UnitSize != 0)
	{
		Boxs.push_back(new VoronoiBoxContainer(Cmin, Cmax));
	}
	else
	{
		if (DoorX != Xunits)
		{
			Boxs.push_back(new VoronoiBoxContainer(Cmin, Cmax - vec3(0, Dimensions.y - DoorMin, 0))); //Bottom
			Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(0, DoorMax, 0), Cmax)); //Top
			if (Cmin.x != Cmax.x - ((Cmax.x - Cmin.x) - DoorX*UnitSize))
				Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(0, DoorMin, 0), Cmax - vec3((Cmax.x - Cmin.x) - DoorX*UnitSize, Dimensions.y - DoorMax, 0))); //CenterA
			if (DoorX*UnitSize + UnitSize + Cmin.x != Cmax.x)
				Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(DoorX*UnitSize + UnitSize, DoorMin, 0), Cmax - vec3(0, Dimensions.y - DoorMax, 0))); //CenterB
		}
		else
		{
			Boxs.push_back(new VoronoiBoxContainer(Cmin, Cmax));
		}
	}
	//ZMaxWall
	if (DoorZ*UnitSize != Dimensions.z)
	{
		Boxs.push_back(new VoronoiBoxContainer(Dmin, Dmax));
	}
	else
	{
		Boxs.push_back(new VoronoiBoxContainer(Dmin, Dmax - vec3(0, Dimensions.y - DoorMin, 0))); //Bottom
		Boxs.push_back(new VoronoiBoxContainer(Dmin + vec3(0, DoorMax, 0), Dmax)); //Top
		if (Dmin.x != Dmax.x - ((Dmax.x - Dmin.x) - DoorX*UnitSize))
			Boxs.push_back(new VoronoiBoxContainer(Dmin + vec3(0, DoorMin, 0), Dmax - vec3((Dmax.x - Dmin.x) - DoorX*UnitSize, Dimensions.y - DoorMax, 0))); //CenterA
		if (DoorX*UnitSize + UnitSize + Dmin.x != Dmax.x)
			Boxs.push_back(new VoronoiBoxContainer(Dmin + vec3(DoorX*UnitSize + UnitSize, DoorMin, 0), Dmax - vec3(0, Dimensions.y - DoorMax, 0))); //CenterB

	}
	SpacersAt = Boxs.size();

	if (Dimensions.x > Dimensions.z)
	{
		//XDimension Largest;
		int Beams = floor((Dimensions.x - RoofRowSpacing) / (RoofRowSpacing + RoofRowSize));
		if (Beams > 0)
		{
			float BeginOffset = (Dimensions.x - Beams*(RoofRowSize)-(Beams + 1)*RoofRowSpacing) / 2;
			float MinBeamY = Max.y - RoofIndent*1.5f - RoofRowSize;
			float MaxBeamY = Max.y - RoofIndent*1.5f;
			float MinBeamZ = Min.z - RoofRowSize;
			float MaxBeamZ = Max.z + RoofRowSize;
			for (int Beam = 0; Beam < Beams; ++Beam)
			{
				float MinBeamX = Min.x + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam)*(RoofRowSize);
				float MaxBeamX = Min.x + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam + 1)*(RoofRowSize);
				Boxs.push_back(new VoronoiBoxContainer(vec3(MinBeamX, MinBeamY, MinBeamZ), vec3(MaxBeamX, MaxBeamY, MaxBeamZ)));
			}
		}
	}
	else
	{
		//ZDimensions Largest;
		int Beams = floor((Dimensions.z - RoofRowSpacing) / (RoofRowSpacing + RoofRowSize));
		if (Beams > 0)
		{
			float BeginOffset = (Dimensions.z - Beams*(RoofRowSize)-(Beams + 1)*RoofRowSpacing) / 2;
			float MinBeamY = Max.y - RoofIndent*1.5f - RoofRowSize;
			float MaxBeamY = Max.y - RoofIndent*1.5f;
			float MinBeamX = Min.x - RoofRowSize;
			float MaxBeamX = Max.x + RoofRowSize;
			for (int Beam = 0; Beam < Beams; ++Beam)
			{
				float MinBeamZ = Min.z + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam)*(RoofRowSize);
				float MaxBeamZ = Min.z + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam + 1)*(RoofRowSize);
				Boxs.push_back(new VoronoiBoxContainer(vec3(MinBeamX, MinBeamY, MinBeamZ), vec3(MaxBeamX, MaxBeamY, MaxBeamZ)));
			}
		}
	}

	for (int Box = 0; Box < Boxs.size(); ++Box)
	{
		for (int s = 0; s < Seeds; ++s)
		{
			Boxs[Box]->AddRandomSeed(VoroType::eSOLID);
		}
	}

	for (int Box = 0; Box < Boxs.size(); ++Box)
	{
		for (int Sphere = 0; Sphere < CutSpheres.size(); ++Sphere)
		{
			Boxs[Box]->SetInsideSphereToType(VoroType::eVOID, glm::vec3(CutSpheres[Sphere]), CutSpheres[Sphere].w);
		}
	}

	for (int Box = 0; Box < Boxs.size(); ++Box)
	{
		Boxs[Box]->CalculateAllSeeds();
	}

}

void GA_OldBuilding::GenBuilding(vec3 Min, vec3 Max, float RoofIndent, float RoofRowSpacing, float RoofRowSize, vec3 DoorLocation, vec3 DoorSize)
{
	vec3 Dimensions = Max - Min;
	float WallWidth = DoorSize.z;
	//Roof
	vec3 RoofMin = vec3(Min.x + WallWidth, Max.y - 2 * RoofIndent, Min.z + WallWidth);
	vec3 RoofMax = vec3(Max.x - WallWidth, Max.y - RoofIndent, Max.z - WallWidth);
	Boxs.push_back(new VoronoiBoxContainer(RoofMin, RoofMax));

	//XminWall
	vec3 Amin = Min;
	vec3 Amax = vec3(Min.x + WallWidth, Max.y, Max.z);
	if (DoorLocation.x != Min.x)
	{
		Boxs.push_back(new VoronoiBoxContainer(Amin, Amax));
	}
	else
	{
		vec3 DoorMin = vec3(DoorLocation.x, DoorLocation.y, DoorLocation.z);
		vec3 DoorMax = DoorMin + vec3(WallWidth, DoorSize.y, DoorSize.x);
	}
	//XMaxWall
	vec3 Bmin = vec3(Max.x - WallWidth, Min.y, Min.z);
	vec3 Bmax = vec3(Max.x, Max.y, Max.z);
	if (DoorLocation.x != Max.x)
	{
		Boxs.push_back(new VoronoiBoxContainer(Bmin, Bmax));
	}
	else
	{

	}
	//ZminWall
	vec3 Cmin = Min;
	vec3 Cmax = vec3(Max.x, Max.y, Min.z + WallWidth);
	if (DoorLocation.z != 0)
	{
		Boxs.push_back(new VoronoiBoxContainer(Cmin, Cmax));
	}
	else
	{
		Boxs.push_back(new VoronoiBoxContainer(Cmin, Cmax - vec3(0, Dimensions.y - DoorLocation.y, 0))); //Bottom
		Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(0, DoorLocation.y + DoorSize.y, 0), Cmax)); //Top
		Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(0, DoorLocation.y, 0), Cmax - vec3((Cmax.x-Cmin.x)-DoorLocation.x, Dimensions.y - DoorLocation.y - DoorSize.y,0))); //CenterA
		Boxs.push_back(new VoronoiBoxContainer(Cmin + vec3(DoorLocation.x + DoorSize.x, DoorLocation.y, 0), Cmax - vec3(0, Dimensions.y - DoorLocation.y - DoorSize.y, 0))); //CenterB
	}
	//ZMaxWall
	vec3 Dmin = vec3(Min.x, Min.y, Max.z - WallWidth);
	vec3 Dmax = vec3(Max.x, Max.y, Max.z);
	if (DoorLocation.z != Dimensions.z)
	{
		Boxs.push_back(new VoronoiBoxContainer(Dmin, Dmax));
	}
	else
	{
		vec3 DoorMin = vec3(DoorLocation.x, DoorLocation.y, DoorLocation.z) - vec3(DoorSize.x, 0, WallWidth);
		vec3 DoorMax = DoorMin + vec3(DoorSize.x, DoorSize.y, WallWidth);
	}

	SpacersAt = Boxs.size();

	if (Dimensions.x > Dimensions.z)
	{
		//XDimension Largest;
		int Beams = floor((Dimensions.x - RoofRowSpacing) / (RoofRowSpacing + RoofRowSize));
		if (Beams > 0)
		{
			float BeginOffset = (Dimensions.x - Beams*(RoofRowSize)-(Beams + 1)*RoofRowSpacing)/2;
			float MinBeamY = Max.y - RoofIndent*1.5f - RoofRowSize;
			float MaxBeamY = Max.y - RoofIndent*1.5f;
			float MinBeamZ = Min.z - RoofRowSize;
			float MaxBeamZ = Max.z + RoofRowSize;
			for (int Beam = 0; Beam < Beams; ++Beam)
			{
				float MinBeamX = Min.x + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam)*(RoofRowSize);
				float MaxBeamX = Min.x + BeginOffset + (Beam + 1)*RoofRowSpacing + (Beam + 1)*(RoofRowSize);
				Boxs.push_back(new VoronoiBoxContainer(vec3(MinBeamX, MinBeamY, MinBeamZ), vec3(MaxBeamX, MaxBeamY, MaxBeamZ)));
			}
		}
	}
	else
	{
		//ZDimensions Largest;
		int Beams = floor((Dimensions.z - RoofRowSpacing) / (RoofRowSpacing + RoofRowSize));
		if (Beams > 0)
		{
			float BeginOffset = (Dimensions.z - Beams*(RoofRowSize)-(Beams + 1)*RoofRowSpacing)/2;
			float MinBeamY = Max.y - RoofIndent*1.5f - RoofRowSize;
			float MaxBeamY = Max.y - RoofIndent*1.5f;
			float MinBeamX = Min.x - RoofRowSize;
			float MaxBeamX = Max.x + RoofRowSize;
			for (int Beam = 0; Beam < Beams; ++Beam)
			{
				float MinBeamZ = Min.z + BeginOffset + (Beam+1)*RoofRowSpacing +(Beam)*(RoofRowSize);
				float MaxBeamZ = Min.z + BeginOffset + (Beam+1)*RoofRowSpacing +(Beam+1)*(RoofRowSize);
				Boxs.push_back(new VoronoiBoxContainer(vec3(MinBeamX, MinBeamY, MinBeamZ), vec3(MaxBeamX, MaxBeamY, MaxBeamZ)));
			}
		}
	}

	for (int Box = 0; Box < Boxs.size(); ++Box)
	{
		Boxs[Box]->AddRandomSeed(VoroType::eSOLID);
		Boxs[Box]->AddRandomSeed(VoroType::eSOLID);
		Boxs[Box]->CalculateAllSeeds();
	}


}

void GA_OldBuilding::Draw()
{
	for (int Box = 0; Box < SpacersAt; ++Box)
	{
		Boxs[Box]->Draw();
	}
}

void GA_OldBuilding::DrawSpacers()
{
	for (int Box = SpacersAt; Box < Boxs.size(); ++Box)
	{
		Boxs[Box]->Draw();
	}
}