#include "ComplexAssesment.h"
#include "Mathamatics3D.h"
#include "Mathamatics1D.h"
#include "Camera.h"
#include "Inputs.h"

ComplexAssesment::ComplexAssesment()
{
	
}


ComplexAssesment::~ComplexAssesment()
{
}


bool ComplexAssesment::update()
{
	if (Application::update())
	{
		return true;
	}
	else
	{
		return false;
	}

}

void ComplexAssesment::SetBoardUp()
{
	for (int x = 0; x < 8; ++x)
	{
		for (int y = 0; y < 8; ++y)
		{
			int ID = y * 8 + x;
			if (y < 3)
				BoardData[ID] = (x % 2 + y % 2) % 2;
			else if (y>4)
				BoardData[ID] = ((x % 2 + y % 2) % 2)*2;
			else
				BoardData[ID] = 0;
		}
	}
}

void  ComplexAssesment::DrawChips()
{
	for (int x = 0; x < 8; ++x)
	{
		for (int y = 0; y < 8; ++y)
		{
			int ID = y * 8 + x;
			if (BoardData[ID] == 1)
				DrawChipLight(vec3(x, 0, y));
			else if (BoardData[ID] == 2)
				DrawChipDark(vec3(x, 0, y));

		}
	}
}

void ComplexAssesment::DrawBoard()
{
	for (int XTile = 0; XTile < 8; ++XTile)
	{
		for (int YTile = 0; YTile < 8; ++YTile)
		{
			bool Black = (XTile % 2 + YTile % 2) % 2;
			if (Black)
				Gizmos::addAABBFilled(glm::vec3(XTile*TileSize, -TileDepth / 2, YTile*TileSize), glm::vec3(0.5f*TileSize, TileDepth / 2, 0.5f*TileSize), glm::vec4(1), nullptr);
			else
				Gizmos::addAABBFilled(glm::vec3(XTile*TileSize, -TileDepth / 2, YTile*TileSize), glm::vec3(0.5f*TileSize, TileDepth / 2, 0.5f*TileSize), glm::vec4(0, 0, 0, 1), nullptr);
		}
	}
}

void ComplexAssesment::DrawChip(glm::vec3 Loc, glm::vec3 Col)
{
	float Height = 0.1f;
	Gizmos::addCylinderFilled(Loc + glm::vec3(0, Height / 2, 0), TileSize*0.4f, Height / 2, 12, glm::vec4(Col,1), nullptr);
}

void ComplexAssesment::DrawChipLight(glm::vec3 Loc)
{
	DrawChip(Loc, glm::vec3(0.5f,0.8f,0.8f));
}
void ComplexAssesment::DrawChipDark(glm::vec3 Loc)
{
	DrawChip(Loc, glm::vec3(0.2f,0.1f,0.1f));
}

vec3 ComplexAssesment::GetMouseOnBoard()
{
	Plane_3D Board = Plane_3D(vec3(0), vec3(0, 1, 0));
	vec2 MouseLoc = vec2(appBasics->AppInputs->dMouseX, appBasics->AppInputs->dMouseY);
	Line_3D Ray = Line_3D(appBasics->AppCamera->GetPos(), appBasics->AppCamera->VectorThroughScreenPoint(MouseLoc, appBasics->ScreenSize));
	return Ray.GetPointFromT(Board.GetIntercept(Ray));
}

vec3 GetCappedToBoardSquare(vec3 Loc)
{
	int X = (int)(Loc.x + 0.5f);
	int Y = (int)(Loc.z + 0.5f);
	X = CapInt(X, 0, 7);
	Y = CapInt(Y, 0, 7);
	return vec3(X, 0, Y);
}

void ComplexAssesment::draw()
{
	DrawBoard();
	DrawChips();
	vec3 Loc = GetMouseOnBoard();
	Gizmos::addAABB(GetCappedToBoardSquare(Loc) + vec3(0,TileDepth/4,0), glm::vec3(0.5f*TileSize, TileDepth / 4, 0.5f*TileSize), vec4(0,1,0,1), nullptr);

}
bool ComplexAssesment::startup()
{
	if (Application::startup())
	{
		SetBoardUp();
		return true;
	}
	else
	{
		return false;
	}
}