#include "VoronoiContainers.h"

VoronoiBoxContainer::VoronoiBoxContainer(vec3 _Min, vec3 _Max)
{
	Min = _Min;
	Max = _Max;
}

VoronoiBoxContainer::~VoronoiBoxContainer()
{
	DeleteCells();
}

void VoronoiBoxContainer::DeleteCells()
{
	for (int i = 0; i < Cells.size(); ++i)
	{
		delete Cells[i];
	}
}

float Random01()
{
	return (float)rand() / RAND_MAX;
}

void VoronoiBoxContainer::AddRandomSeed(VoroType TypeRef)
{
	float XRange = Max.x - Min.x;
	float YRange = Max.y - Min.y;
	float ZRange = Max.z - Min.z;

	vec3 Loc = vec3(Min.x + XRange*Random01(), Min.y + YRange*Random01(), Min.z + ZRange*Random01());
	VoronoiCell* NewCell = new VoronoiCell(Loc,1);
	NewCell->Type = TypeRef;
	Cells.push_back(NewCell);
}

void VoronoiBoxContainer::CalculateAllSeeds()
{
	for (int i = 0; i<Cells.size();++i)
	{
		if (Cells[i]->Type > VoroType::eVOID)
		{
			for (int j = 0; j < Cells.size();++j)
			{
				if (Cells[i] != Cells[j])
				{
					Cells[i]->GenFaceFromSeed(Cells[j]);
				}
			}
			Cells[i]->AddBoundingBox(Min, Max, true);
			Cells[i]->CopyFaceEdges();
			Cells[i]->Gen_GL_Buffers();
		}
	}
}

void VoronoiBoxContainer::SetInsideSphereToType(VoroType TypeRef, vec3 Center, float Radius)
{
	for (auto CellRef : Cells)
	{
		if (glm::length(CellRef->Location - Center) < Radius)
		{
			CellRef->Type = TypeRef;
		}
	}
	
}

void VoronoiBoxContainer::Draw()
{
	for (auto CellRef : Cells)
	{
		CellRef->Draw();
	}
	//DrawEdges(vec4(1));
}

void VoronoiBoxContainer::DrawEdges(vec4 Col)
{
	for (auto CellRef : Cells)
	{
		CellRef->DrawEdges(Col);
	}
}