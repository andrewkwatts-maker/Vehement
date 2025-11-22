#include "VORO_Space.h"
#include "aie\Gizmos.h"
#include "glm\mat4x4.hpp"
VORO_Space::VORO_Space(float GridSize)
{
	MapCellSize = GridSize;
	Map = new Expandable3D_deque < MapCell >;
}


VORO_Space::~VORO_Space()
{
	for (int X = Map->GetMinX(); X < Map->GetMaxX();++X)
	{
		for (int Y = Map->GetMinY(); Y < Map->GetMaxY(); ++Y)
		{
			for (int Z = Map->GetMinZ(); Z < Map->GetMaxZ(); ++Z)
			{
				delete Map->GetAtQuick(X, Y, Z);
			}
		}
	}
	delete Map;
}

void VORO_Space::Build(vec3 InspectionPos, VoroType PlaceThis)
{
	Gizmos::addTransform(glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(InspectionPos, 1)));
	vec3 MapRefrence = InspectionPos / MapCellSize;
	MapRefrence = vec3(floor(MapRefrence.x), floor(MapRefrence.y), floor(MapRefrence.z));
	vec3 MapCellLoc = MapRefrence*MapCellSize;
	Gizmos::addAABB(MapCellLoc + vec3(MapCellSize) / 2.0f, vec3(MapCellSize / 2), glm::vec4(1), nullptr);
	Gizmos::addAABB(vec3(Map->GetMinX(), Map->GetMinY(), Map->GetMinZ()) + vec3(Map->GetSizeX(), Map->GetSizeY(), Map->GetSizeZ()) / 2.0f, vec3(Map->GetSizeX(), Map->GetSizeY(), Map->GetSizeZ())/2.0f, glm::vec4(1,0,1,1), nullptr);


	//FillMap
	while (MapRefrence.x < Map->GetMinX())
	{
		Map->Add_MinX();
	}
	while (MapRefrence.y < Map->GetMinY())
	{
		Map->Add_MinY();
	}
	while (MapRefrence.z < Map->GetMinZ())
	{
		Map->Add_MinZ();
	}
	while (MapRefrence.x >= Map->GetMaxX())
	{
		Map->Add_MaxX();
	}
	while (MapRefrence.y >= Map->GetMaxY())
	{
		Map->Add_MaxY();
	}
	while (MapRefrence.z >= Map->GetMaxZ())
	{
		Map->Add_MaxZ();
	}

	MapCell* Cell = Map->GetAt((int)MapRefrence.x, (int)MapRefrence.y, (int)MapRefrence.z);

	for (int CellId = 0; CellId < Cell->VoroCells.size(); ++CellId)
	{
		Gizmos::addTransform(glm::mat4(glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1), glm::vec4(Cell->VoroCells[CellId]->Location, 1)));
		Cell->VoroCells[CellId]->DrawEdges(vec3(1, 0, 0));
	}
	//
	if (PlaceThis != VoroType::eEmpty)
	{
		Cell->VoroCells.push_back(new VORO_CELL(InspectionPos, 1, PlaceThis));
		Recalculate(Cell->VoroCells[Cell->VoroCells.size() - 1]);
	}

}

VORO_CELL* VORO_Space::DrawInspectionEffect(vec3 InspectionPos)
{

	vec3 MapRefrence = InspectionPos / MapCellSize;
	MapRefrence = vec3(floor(MapRefrence.x), floor(MapRefrence.y), floor(MapRefrence.z));
	vector<MapCell*> MapResults = Map->GetCubeAt(MapRefrence.x, MapRefrence.y, MapRefrence.z);

	VORO_CELL* Cell = new VORO_CELL(InspectionPos, 1, VoroType::eVOID);
	Cell->GenBoundingBox(MapCellSize*2);
	//CalculateThisCell
	float MaxSize = 0;
	for (int i = 0; i < MapResults.size(); ++i)
	{
		if (MapResults[i]->VoroCells.size()> MaxSize)
			MaxSize = MapResults[i]->VoroCells.size();
	}

	for (int i = 0; i < MaxSize; ++i)
	{
		for (int j = 0; j < MapResults.size(); ++j)
		{
			if (i < MapResults[j]->VoroCells.size())
			{
				if (Cell != MapResults[j]->VoroCells[i])
					Cell->AddSeed(MapResults[j]->VoroCells[i]);
			}
		}
	}

	//for (int X = Map->GetMinX(); X < Map->GetMaxX(); ++X)
	//{
	//	for (int Y = Map->GetMinY(); Y < Map->GetMaxY(); ++Y)
	//	{
	//		for (int Z = Map->GetMinZ(); Z < Map->GetMaxZ(); ++Z)
	//		{
	//			for (int C = 0; C<Map->GetAtQuick(X, Y, Z)->VoroCells.size(); ++C)
	//			{
	//				if (Cell != Map->GetAtQuick(X, Y, Z)->VoroCells[C])
	//					Cell->AddSeed(Map->GetAtQuick(X, Y, Z)->VoroCells[C]);
	//			}
	//		}
	//	}
	//}

	return Cell;
}

void VORO_Space::Recalculate(VORO_CELL* Cell)
{
	vector<VORO_CELL*> RecalculationsNeeded;

	for (int i = 0; i < Cell->Faces.size(); ++i)
	{
		VORO_CELL* CELLREF = (VORO_CELL*)Cell->Faces[i]->FormingSeed_Other;
		if (CELLREF != nullptr)
			RecalculationsNeeded.push_back(CELLREF);
	}

	Cell->Delete_LeaveSeed();
	Cell->GenBoundingBox(MapCellSize * 2);
	Cell->GenBoundingRadius();

	vec3 MapRefrence = Cell->Location / MapCellSize;
	MapRefrence = vec3(floor(MapRefrence.x), floor(MapRefrence.y), floor(MapRefrence.z));
	vector<MapCell*> MapResults = Map->GetCubeAt(MapRefrence.x, MapRefrence.y, MapRefrence.z);

	//CalculateThisCell
	float MaxSize = 0;
	for (int i = 0; i < MapResults.size(); ++i)
	{
		if (MapResults[i]->VoroCells.size()> MaxSize)
			MaxSize = MapResults[i]->VoroCells.size();
	}

	for (int i = 0; i < MaxSize; ++i)
	{
		for (int j = 0; j < MapResults.size(); ++j)
		{
			if (i < MapResults[j]->VoroCells.size())
			{
				if (Cell != MapResults[j]->VoroCells[i])
					Cell->AddSeed(MapResults[j]->VoroCells[i]);
			}
		}
	}
	Cell->Gen_GL_Buffers();

	//Find New Cells
	for (int i = 0; i < Cell->Faces.size(); ++i)
	{
		VORO_CELL* CELLREF = (VORO_CELL*)Cell->Faces[i]->FormingSeed_Other;
		if (CELLREF != nullptr)
		{
			bool Acceptable = true;
			for (int j = 0; j < RecalculationsNeeded.size(); ++j)
			{
				if (CELLREF == RecalculationsNeeded[j])
					Acceptable = false;
			}

			if (Acceptable)
				RecalculationsNeeded.push_back(CELLREF);
		}
	}

	//
	for (int Recal = 0; Recal < RecalculationsNeeded.size(); ++Recal)
	{
		VORO_CELL* CELLID = RecalculationsNeeded[Recal];

		CELLID->Delete_LeaveSeed();
		CELLID->GenBoundingBox(MapCellSize * 2);

		vec3 MapRefrence2 = CELLID->Location / MapCellSize;
		MapRefrence2 = vec3(floor(MapRefrence2.x), floor(MapRefrence2.y), floor(MapRefrence2.z));
		vector<MapCell*> MapResults2 = Map->GetCubeAt(MapRefrence2.x, MapRefrence2.y, MapRefrence2.z);

		float MaxSize2 = 0;
		for (int i = 0; i < MapResults2.size(); ++i)
		{
			if (MapResults2[i]->VoroCells.size()> MaxSize2)
				MaxSize2 = MapResults2[i]->VoroCells.size();
		}

		for (int i = 0; i < MaxSize2; ++i)
		{
			for (int j = 0; j < MapResults2.size(); ++j)
			{
				if (i < MapResults2[j]->VoroCells.size())
				{
					if (MapResults2[j]->VoroCells[i] != CELLID)
						CELLID->AddSeed(MapResults2[j]->VoroCells[i]);
				}
			}
		}
		CELLID->Gen_GL_Buffers();
	}
};

void VORO_Space::Draw()
{
	for (int X = Map->GetMinX(); X < Map->GetMaxX(); ++X)
	{
		for (int Y = Map->GetMinY(); Y < Map->GetMaxY(); ++Y)
		{
			for (int Z = Map->GetMinZ(); Z < Map->GetMaxZ(); ++Z)
			{
				for (int C = 0; C<Map->GetAtQuick(X, Y, Z)->VoroCells.size(); ++C)
				{
					Map->GetAtQuick(X, Y, Z)->VoroCells[C]->Draw();
				}
			}
		}
	}
}