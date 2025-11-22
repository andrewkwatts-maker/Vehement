#pragma once
#include "Expandable3D_deque.h"
#include <vector>
#include "VORO_CELL.h"
class MapCell;
class VORO_Space
{
public:
	void Build(vec3 InspectionPos, VoroType PlaceThis); //Doesn't Place if Empty
	VORO_CELL* DrawInspectionEffect(vec3 InspectionPos);
	void Draw(); //Drawring Of Whats in Grid

	VORO_Space(float GridSize);
	~VORO_Space();

private:
	Expandable3D_deque<MapCell>* Map;
	float MapCellSize;
	void Recalculate(VORO_CELL* Cell);
};




class MapCell
{
public:
	MapCell()
	{
		TypesInCell = eEmpty;
	}

	~MapCell()
	{
		for (int Cell = 0; Cell < VoroCells.size(); ++Cell)
		{
			delete VoroCells[Cell];
		}
	}
	vector<VORO_CELL*> VoroCells;
	VoroType TypesInCell;

	void CheckTypes()
	{
		VoroType TypeA;
		if (VoroCells.size() > 0)
		{
			TypeA = VoroCells[0]->Type;
			for (int i = 1; i < VoroCells.size(); ++i)
			{
				if (VoroCells[i]->Type != TypeA)
				{
					TypesInCell = eMixed;
					return;
				}
			}
			TypesInCell = TypeA;
		}
		else
			TypesInCell = eEmpty;
	}
};