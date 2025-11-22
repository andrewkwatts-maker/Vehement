#include "GA_BuildingCluster.h"
#include "GL_Manager.h"
#include "glm\glm.hpp"
#include "glm\gtc\matrix_transform.inl"
#include "Mathamatics1D.h"
GA_BuildingCluster::GA_BuildingCluster()
{
	Base = nullptr;
}


GA_BuildingCluster::~GA_BuildingCluster()
{
	for (int i = 0; i < Cluster.size(); ++i)
	{
		delete Cluster[i];
	}
	delete Base;
}

void GA_BuildingCluster::Draw(vec3 LOC,GL_Manager* Manager)
{
	for (int i = 0; i < Cluster.size(); ++i)
	{
		Manager->SetTransform(glm::translate(glm::mat4(),Locs[i]+LOC));
		Cluster[i]->Draw();
	}
	Manager->SetTransform(glm::translate(glm::mat4(), LOC));
	Base->Draw();
}

void GA_BuildingCluster::DrawSpacers(vec3 LOC, GL_Manager* Manager)
{
	for (int i = 0; i < Cluster.size(); ++i)
	{
		Manager->SetTransform(glm::translate(glm::mat4(), Locs[i] + LOC));
		Cluster[i]->DrawSpacers();
	}
}

void  GA_BuildingCluster::ApplyAge(float Age, float RoofHeight, int Seed, int Xsize, int Ysize)
{
	Age *= 0.5f;
	srand(Seed);
	if (Age > 0.5f);

	float Var = Age - 0.5f;
	Var += 1;
	Var = sqrt(Var);
	Var	-= 1;
	Age = 0.5f + Var;

	int Cuts = 0;
	vector<vec4> Cutouts;
	if (Age != 0)
	{
		Cuts = Age * 5;
		for (int Cut = 0; Cut < Cuts; ++Cut)
		{
			int Size = Age * 50 - Cut*10;
			float SizeFinal = (float)Size / 10;
			float Height = RoofHeight + 0.5f;
			if (Cluster.size()>1)
			{
				Height = 2 * RoofHeight + 0.5f;
			}
			Height -= 1;
			Cutouts.push_back(vec4(rand() % Xsize, Height, rand() % Ysize, SizeFinal));
		}
	}

	Cluster[0]->ApplyCuts(Cutouts);
	if (Cluster.size() > 1)
	{
		for (int cut = 0; cut < Cuts; ++cut)
		{
			Cutouts[cut].y -= RoofHeight;
		}
		Cluster[1]->ApplyCuts(Cutouts);
	}

}

vec4 GA_BuildingCluster::Build(float Age, int Fractures)
{
	Age = 0;

	bool UpperRoof = rand() % 2;

	//Lower
	GA_OldBuilding* a = new GA_OldBuilding();
	int Xa = CapInt(rand() % 5 + 4,4,6);
	int Ya = CapInt(rand() % 5 + 4, 4, 6);

	//Upper If Needed
	int Xb = rand() % (Xa - 3) + 2;
	int Yb = rand() % (Ya - 3) + 2;
	Xb = CapInt(Xb, 2, Xa - 1);
	Yb = CapInt(Yb, 2, Ya - 1);
	int XDb = rand() % (Xb - 1);
	int YDb = rand() % Yb;
	int Difx = Xa - Xb;
	int Dify = Ya - Yb;
	float Locx = rand() % (Difx + 1);
	float Locy = rand() % (Dify + 1);

	//LowerContinued
	if (!UpperRoof)
	{
		Xa -= rand() % 3;
		Ya -= rand() % 3;
	}
	int XDa = rand() % (Xa - 1);
	int YDa = rand() % Ya;

	float RoofHeight = 3;
	float RoofIndent = 0.2f;
	float WallWidth = 0.2f;
	float BarSpacing = 0.5f;
	int Cuts = 0;
	vector<vec4> Cutouts;
	//if (Age != 0)
	//{
	//	Cuts = rand() % ((int)(Age*9) + 2) + Age*3;
	//	for (int Cut = 0; Cut < Cuts; ++Cut)
	//	{
	//		int Size = rand() % 20 + Age*30;
	//		float SizeFinal = (float)Size / 10;
	//		float Height = RoofHeight+0.5f;
	//		if (UpperRoof)
	//		{
	//			Height = 2 * RoofHeight+0.5f;
	//		}
	//		Height -= 1;
	//		Cutouts.push_back(vec4(rand() % Xa, Height, rand() % Ya, SizeFinal));
	//	}
	//}



	a->GenBuilding(Xa, Ya, 1, RoofHeight, RoofIndent, BarSpacing, 0.2f, XDa, YDa, 0.2f, RoofHeight*0.7f, WallWidth, Fractures, Cutouts);

	Cluster.push_back(a);
	Locs.push_back(vec3(0));

	if (UpperRoof)
	{
		GA_OldBuilding* b = new GA_OldBuilding();
		for (int cut = 0; cut < Cuts; ++cut)
		{
			Cutouts[cut].y -= RoofHeight;
		}

		b->GenBuilding(Xb, Yb, 1, RoofHeight, RoofIndent, BarSpacing, 0.2f, XDb, YDb, 0.2f, RoofHeight*0.7f, WallWidth, Fractures,Cutouts);


		if (Locx == 0)
			Locx = WallWidth;
		if (Locx == Xa - Xb)
			Locx = Xa - Xb - WallWidth;
		if (Locy == 0)
			Locy = WallWidth;
		if (Locy == Ya - Yb)
			Locy = Ya - Yb - WallWidth;

		Cluster.push_back(b);
		Locs.push_back(vec3(Locx,RoofHeight-RoofIndent,Locy));
	}

	Base = new VoronoiBoxContainer(vec3(0, -7*RoofHeight, 0), vec3(Xa, 0, Ya));
	for (int i = 0; i < 2; ++i)
	{
		Base->AddRandomSeed(VoroType::eSOLID);
	}
	Base->CalculateAllSeeds();

	Details = vec4(Xa, Ya, XDa, YDa);
	return Details;

}

vec4  GA_BuildingCluster::WholeBuild()
{
	bool UpperRoof = rand() % 2;

	GA_OldBuilding* a = new GA_OldBuilding();
	int Xa = rand() % 3 + 4;
	int Ya = rand() % 3 + 4;
	if (!UpperRoof)
	{
		Xa -= rand() % 3;
		Ya -= rand() % 3;
	}
	int XDa = rand() % (Xa - 1);
	int YDa = rand() % Ya;
	float RoofHeight = 3;
	float RoofIndent = 0.2f;
	float WallWidth = 0.2f;
	float BarSpacing = 0.5f;
	int Cuts = 0;
	vector<vec4> Cutouts;

	a->GenBuilding(Xa, Ya, 1, RoofHeight, RoofIndent, BarSpacing, 0.2f, XDa, YDa, 0.2f, RoofHeight*0.7f, WallWidth, 2, Cutouts);

	Cluster.push_back(a);
	Locs.push_back(vec3(0));

	if (UpperRoof)
	{
		GA_OldBuilding* b = new GA_OldBuilding();
		int Xb = rand() % (Xa - 3) + 2;
		int Yb = rand() % (Ya - 3) + 2;
		Xb = CapInt(Xb, 2, Xa - 1);
		Yb = CapInt(Yb, 2, Ya - 1);
		int XDb = rand() % (Xb - 1);
		int YDb = rand() % Yb;

		for (int cut = 0; cut < Cuts; ++cut)
		{
			Cutouts[cut].y -= RoofHeight;
		}

		b->GenBuilding(Xb, Yb, 1, RoofHeight, RoofIndent, BarSpacing, 0.2f, XDb, YDb, 0.2f, RoofHeight*0.7f, WallWidth, 2, Cutouts);

		int Difx = Xa - Xb;
		int Dify = Ya - Yb;

		float Locx = rand() % (Difx + 1);
		float Locy = rand() % (Dify + 1);
		if (Locx == 0)
			Locx = WallWidth;
		if (Locx == Xa - Xb)
			Locx = Xa - Xb - WallWidth;
		if (Locy == 0)
			Locy = WallWidth;
		if (Locy == Ya - Yb)
			Locy = Ya - Yb - WallWidth;

		Cluster.push_back(b);
		Locs.push_back(vec3(Locx, RoofHeight - RoofIndent, Locy));
	}

	Base = new VoronoiBoxContainer(vec3(0, -7*RoofHeight, 0), vec3(Xa, 0, Ya));
	for (int i = 0; i < 2; ++i)
	{
		Base->AddRandomSeed(VoroType::eSOLID);
	}
	Base->CalculateAllSeeds();

	Details = vec4(Xa, Ya, XDa, YDa);
	return Details;
}