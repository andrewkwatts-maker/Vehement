#pragma once
#include "Application.h"
class ComplexAssesment :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	ComplexAssesment();
	~ComplexAssesment();

private:
	int BoardData[8*8];

	void SetBoardUp();


	float TileSize = 1;
	float TileDepth = 0.4f;

	void DrawBoard();
	void DrawChips();
	void DrawChip(glm::vec3 Loc, glm::vec3 Col);
	void DrawChipLight(glm::vec3 Loc);
	void DrawChipDark(glm::vec3 Loc);
	glm::vec3 GetMouseOnBoard();



};

