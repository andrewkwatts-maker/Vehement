#pragma once
#include "Application.h"
#include "FlowNode.h"
class ComplexTut6:public Application 
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	bool BootFlowField();
	bool ShutdownFlowField();

	bool UpdateField();
	void DrawField();

	void calculatePathDistances(glm::vec2 Goal);
	void calculateFlowField();

	ComplexTut6();
	~ComplexTut6();

private:
	float m_prev_Time;
	FlowNode* m_Grid;
	glm::vec3* m_FlowField;
	glm::vec2 m_Target;

	int RowCount;
	int ColCount;

	bool CanPathToGoal(int X, int Y,float CareFactor);

	FlowBot m_Bot;
};

