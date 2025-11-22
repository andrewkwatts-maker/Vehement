#pragma once
#include "Application.h"
#include "VORO_Space.h"


//class MapCell
//{
//public:
//	MapCell()
//	{
//		TypesInCell = eEmpty;
//	}
//	vector<VORO_CELL*> VoroCells;
//	VoroType TypesInCell;
//
//	void CheckTypes()
//	{
//		VoroType TypeA;
//		if (VoroCells.size() > 0)
//		{
//			TypeA = VoroCells[0]->Type;
//			for (int i = 1; i < VoroCells.size(); ++i)
//			{
//				if (VoroCells[i]->Type != TypeA)
//				{
//					TypesInCell = eMixed;
//					return;
//				}
//			}
//			TypesInCell = TypeA;
//		}
//		else
//			TypesInCell = eEmpty;
//	}
//};

class VoroExample2 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	VORO_Space* Space;


	//GLData
	int PointTexturedBump;
	int RockDiffuse;
	int RockNormal;

	//Expandable3D_deque<MapCell>* Map;
	//float MapCellSize = 10;
	//
	//void Recalculate(Expandable3D_deque<MapCell>* MapRef,VORO_CELL* Cell,float Size);
	//
	//vector<VORO_CELL*> NeedingCalculation;

	VoroExample2();
	~VoroExample2();
};

