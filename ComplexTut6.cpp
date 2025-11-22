#include "ComplexTut6.h"
#include <list>
#include "Inputs.h"
#include "Camera.h"
#include <vector>
ComplexTut6::ComplexTut6()
{
}


ComplexTut6::~ComplexTut6()
{
}


bool ComplexTut6::update()
{
	if (Application::update())
	{
		UpdateField();
		return true;
	}
	else
	{
		return false;
	}
}
void ComplexTut6::draw()
{
	DrawField();
	//Application::draw();
}
bool ComplexTut6::startup()
{
	if (Application::startup())
	{
		BootFlowField();

		return true;
	}
	else
	{
		return false;
	}

}



bool ComplexTut6::BootFlowField()
{
	RowCount = 100;
	ColCount = 100;
	m_Grid = new FlowNode[RowCount*ColCount];

	int i = 0;
	for (int row = -RowCount / 2; row < RowCount / 2; ++row)
	{
		for (int col = -ColCount / 2; col < ColCount / 2; ++col)
		{
			m_Grid[i].Position = glm::vec3(col, 0, row);
			m_Grid[i].Wall = false;
			i++;
		}
	}

	for (int r = 0; r < RowCount; ++r)
	{
		for (int c = 0; c < ColCount; ++c)
		{
			int idx = 0;
			int cell = (r*ColCount) + c;

			if (r>0)
				m_Grid[cell].Edges[idx++] = &m_Grid[((r - 1)*ColCount) + c];
			if (r < RowCount - 1)
				m_Grid[cell].Edges[idx++] = &m_Grid[((r + 1)*ColCount) + c];
			if (c>0)
				m_Grid[cell].Edges[idx++] = &m_Grid[(r*ColCount) + (c - 1)];
			if (c < ColCount - 1)
				m_Grid[cell].Edges[idx++] = &m_Grid[(r*ColCount) + (c + 1)];

			for (; idx < 4; idx++)
				m_Grid[cell].Edges[idx] = nullptr;

		}
	}

	m_FlowField = new glm::vec3[RowCount*ColCount];
	m_Target = glm::vec2(20, 20);
	m_Bot.Position = glm::vec3(-24, 0, -24);
	return true;
};
bool ComplexTut6::ShutdownFlowField()
{
	return true;
};

bool ComplexTut6::UpdateField()
{
	calculatePathDistances(m_Target);
	calculateFlowField();

	if (appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_1) == true || appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_2) == true) //left
	{

		bool ResultA = appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_1);
		bool ResultB = appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_2);

		double x, y;
		int w, h;
		
		x = appBasics->AppInputs->dMouseX;
		y = appBasics->AppInputs->dMouseY;
		
		w = appBasics->ScreenSize.x;
		h = appBasics->ScreenSize.y;
		
		glm::vec4 screen = glm::vec4(2.0f*(x / w) - 1.0f, 1.0f - (2.0f*(y / h)), 1, 1);
		glm::vec4 pos = glm::inverse(appBasics->AppCamera->GetProjectionView())*screen;
		
		pos.w = 1.0f / pos.w;
		pos.x *= pos.w;
		pos.y *= pos.w;
		pos.z *= pos.w;

		//glm::vec3 vForward = glm::normalize(appBasics->AppCamera->GetPos() - vec3(pos));
		glm::vec3 vForward = appBasics->AppCamera->VectorThroughScreenPoint(appBasics->AppInputs->MouseLoc(),appBasics->ScreenSize);

		float d = glm::dot(vec3(0), vec3(0, 1, 0));
		float t = (d - glm::dot(vec3(pos), vec3(0, 1, 0))) / glm::dot(vForward, vec3(0, 1, 0));
		vec3 intersect = vec3(pos) + (t*vForward);

		if (intersect.x > -ColCount / 2 && intersect.x < ColCount / 2 && intersect.z > -RowCount / 2 && intersect.z < RowCount / 2)
		{
			int idx = ((int)intersect.z + RowCount / 2)*ColCount + ((int)intersect.x + ColCount / 2);

			if (appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_2) == true)
			{
				//m_Target = glm::vec2(idx%ColCount, idx / ColCount);
				m_Grid[idx].Wall = true;
			}
			else if (appBasics->AppInputs->IsMouseDown(GLFW_MOUSE_BUTTON_1) == true)
			{
				m_Target = glm::vec2(idx%ColCount, idx / ColCount);
			}
		}

	}


	return true;
};
void ComplexTut6::DrawField()
{
	//Draw Grid
	int rows = RowCount + 1;
	int halfRow = rows / 2;
	for (int i = 0; i < rows; ++i)
	{
		Gizmos::addLine(glm::vec3(-halfRow + i - 0.5f, 0, halfRow - 0.5f), glm::vec3(-halfRow + i - 0.5f, 0, -halfRow - 0.5f), i == halfRow ? glm::vec4(0.5f):glm::vec4(0, 0, 0, 1));
	}


	int cols = ColCount + 1;
	int halfCol = cols / 2;
	for (int i = 0; i < rows; ++i)
	{
		Gizmos::addLine(glm::vec3(-halfCol - 0.5f, 0, -halfCol + i - 0.5f), glm::vec3(halfCol - 0.5f, 0, -halfRow +i - 0.5f), i == halfCol ? glm::vec4(0.5f) : glm::vec4(0, 0, 0, 1));
	}

	//Draw Flow Field Vectors
	for (int i = 0; i < ColCount*RowCount; ++i)
	{
		Gizmos::addLine(m_Grid[i].Position, m_Grid[i].Position + m_FlowField[i] * 0.5f, glm::vec4(0, 1, 1, 1));
		//for (int j = 0; j < 4; ++j)
		//{
		//	if (m_Grid[i].Edges[j] != nullptr)
		//	{
		//		Gizmos::addLine(m_Grid[i].Position, m_Grid[i].Position + m_FlowField[i] * 0.5f, glm::vec4(0, 1, 1, 1));
		//	}
		//}
	}

	//Draw Walls
	for (int i = 0; i < ColCount*RowCount; ++i)
	{
		if (m_Grid[i].Wall == true)
		{
			Gizmos::addAABB(m_Grid[i].Position, glm::vec3(0.1f), glm::vec4(1, 0, 1, 1));
		}
	}

	//DrawTarget
	Gizmos::addAABB(m_Grid[(int)(m_Target.y*RowCount + m_Target.x)].Position, glm::vec3(0.1f), glm::vec4(1, 1, 0, 1));
};

void ComplexTut6::calculatePathDistances(glm::vec2 Goal)
{
	for (int i = 0; i < RowCount*ColCount; ++i)
	{
		m_Grid[i].Score = 0;
		m_Grid[i].Visited = false;
	}

	FlowNode* goalNode = &m_Grid[(int)Goal.y*RowCount + (int)Goal.x];
	FlowNode* goalNodeB = &m_Grid[((int)Goal.y+1)*RowCount + ((int)Goal.x+1)];
	FlowNode* goalNodeC = &m_Grid[((int)Goal.y+1)*RowCount + (int)Goal.x];
	FlowNode* goalNodeD = &m_Grid[(int)Goal.y*RowCount + ((int)Goal.x+1)];
	std::list<FlowNode*> Open;

	Open.push_back(goalNode);
	Open.push_back(goalNodeB);
	Open.push_back(goalNodeC);
	Open.push_back(goalNodeD);

	while (Open.empty() == false)
	{
		FlowNode* N = Open.front();
		Open.pop_front();

		if (N->Visited)
			continue;

		N->Visited = true;

		for (int i = 0; i < 4; ++i)
		{
			FlowNode* edge = N->Edges[i];

			if (edge != nullptr && !edge->Visited && edge->Wall == false)
			{
				edge->Score = N->Score + 1;
				Open.push_back(edge);
			}
		}

	}
};
void ComplexTut6::calculateFlowField()
{
	for (int row = 0; row < RowCount; ++row)
	{
		for (int col = 0; col < ColCount; ++col)
		{
			int idx = row*ColCount + col;

			if (m_Grid[idx].Wall)
			{
				m_FlowField[idx] = glm::vec3(0);
				continue;
			}


			if ((col == 0 || m_Grid[idx - 1].Wall) && (col == ColCount - 1 || m_Grid[idx + 1].Wall))
				m_FlowField[idx].x = 0;
			else if (col == 0 || m_Grid[idx - 1].Wall)
				m_FlowField[idx].x = m_Grid[idx].Score - m_Grid[idx + 1].Score;
			else if (col == ColCount - 1 || m_Grid[idx + 1].Wall)
				m_FlowField[idx].x = m_Grid[idx - 1].Score - m_Grid[idx].Score;
			else
				m_FlowField[idx].x = m_Grid[idx - 1].Score - m_Grid[idx+1].Score;


			if ((row == 0 || m_Grid[idx - RowCount].Wall) && (row == RowCount - 1 || m_Grid[idx + RowCount].Wall))
				m_FlowField[idx].z = 0;
			else if (row == 0 || m_Grid[idx - RowCount].Wall)
				m_FlowField[idx].z = m_Grid[idx].Score - m_Grid[idx + RowCount].Score;
			else if (row == RowCount - 1 || m_Grid[idx + RowCount].Wall)
				m_FlowField[idx].z = m_Grid[idx - RowCount].Score - m_Grid[idx].Score;
			else
				m_FlowField[idx].z = m_Grid[idx - RowCount].Score - m_Grid[idx + RowCount].Score;

			m_FlowField[idx].y = 0;

			if (m_FlowField[idx].x == 0 && m_FlowField[idx].y == 0 && m_FlowField[idx].z == 0)
				continue;

			m_FlowField[idx] = glm::normalize(m_FlowField[idx]);

		}
	}

	////Key Points - My Additional Code;
	//std::vector<int> KeyPoints; //where their is a courner condition;
	//
	////Add Goal
	//KeyPoints.push_back((int)m_Target.y*RowCount + (int)m_Target.x);
	//
	//for (int row = 0; row < RowCount; ++row)
	//{
	//	for (int col = 0; col < ColCount; ++col)
	//	{
	//		int idx = row*ColCount + col;
	//		int idx_A = (row - 1)*ColCount + col -1;
	//		int idx_B = (row - 1)*ColCount + col + 1;
	//		int idx_C = (row + 1)*ColCount + col - 1;
	//		int idx_D = (row + 1)*ColCount + col + 1;
	//
	//		int idx_negY = (row - 1)*ColCount + col;
	//		int idx_posY = (row + 1)*ColCount + col;
	//		int idx_negX = row*ColCount + col -1;
	//		int idx_posX = row*ColCount + col + 1;
	//
	//		bool KeyPoint = false;
	//		if (m_Grid[idx].Score >0) //knock out un-travelable nodes such as walls and cells enclosed by walls
	//		{
	//			if (m_Grid[idx_A].Wall == true && m_Grid[idx_negX].Wall == false && m_Grid[idx_negY]. Wall == false)
	//			{
	//				KeyPoint = true;
	//			}
	//			else if (m_Grid[idx_B].Wall == true && m_Grid[idx_negY].Wall == false && m_Grid[idx_posX].Wall == false)
	//			{
	//				KeyPoint = true;
	//			}
	//			else if (m_Grid[idx_C].Wall == true && m_Grid[idx_negX].Wall == false && m_Grid[idx_posY].Wall == false)
	//			{
	//				KeyPoint = true;
	//			}
	//			else if (m_Grid[idx_D].Wall == true && m_Grid[idx_posX].Wall == false && m_Grid[idx_posY].Wall == false)
	//			{
	//				KeyPoint = true;
	//			}
	//		}
	//		if (KeyPoint == true)
	//		{
	//			KeyPoints.push_back(idx);
	//		}
	//
	//	}
	//}

	//GetBetterflowPath
	//for (int row = 0; row < RowCount; ++row)
	//{
	//	for (int col = 0; col < ColCount; ++col)
	//	{
	//		int IDX = row*ColCount + col;
	//		if (CanPathToGoal(row, col,10))
	//		{
	//			m_FlowField[IDX] = glm::normalize(vec3(m_Target.x - col,0,m_Target.y - row));
	//		}
	//		//else
	//		//{
	//		//	std::vector<int> Accaptable;
	//		//
	//		//	for (int i = 1; i < KeyPoints.size(); ++i)
	//		//	{
	//		//		if (m_Grid[KeyPoints[i]].Score < m_Grid[IDX].Score)
	//		//		{
	//		//			vec3 Delta = m_Grid[KeyPoints[i]].Position - m_Grid[IDX].Position;
	//		//			float DotProd = glm::dot(Delta, m_FlowField[IDX]);
	//		//			if (DotProd >= pow(2, 0.5f))
	//		//				Accaptable.push_back(KeyPoints[i]);
	//		//		}
	//		//	}
	//		//
	//		//	int UseID = -1;
	//		//	int MaxScore = 0;
	//		//	for (int j = 0; j < Accaptable.size(); j++)
	//		//	{
	//		//		if (m_Grid[Accaptable[j]].Score > MaxScore)
	//		//			UseID = Accaptable[j];
	//		//	}
	//		//
	//		//	if (UseID >= 0)
	//		//	{
	//		//		m_FlowField[IDX] = glm::normalize(m_Grid[UseID].Position - m_Grid[IDX].Position);
	//		//	}
	//		//}
	//	}
	//}


};

#include <minmax.h>
bool ComplexTut6::CanPathToGoal(int X, int Y,float CareFactor)
{
	int GoalX = (int)m_Target.y;
	int GoalY = (int)m_Target.x;

	int Minx = min(X, GoalX);
	int Miny = min(Y, GoalY);
	int Maxx = max(X, GoalX);
	int Maxy = max(Y, GoalY);

	vec2 Delta = vec2(Maxx - Minx, Maxy - Miny);
	if (Delta.length() > CareFactor)
	{
		return false;
	}

	if (m_Grid[Miny*ColCount + Minx].Wall == true)
		return false;
	if (m_Grid[Maxy*ColCount + Maxx].Wall == true)
		return false;

	vec2 StepTrack = vec2(0, 0);
	int Steps = Delta.x + Delta.y;

	bool EncounteredWall = false;

	for (int Step = 0; Step < Steps; ++Step)
	{
		if (EncounteredWall == false)
		{
			int IDX = ((int)StepTrack.y + Miny)*ColCount + (int)StepTrack.x + Minx;
			if (m_Grid[IDX].Wall == true)
			{
				return false;
			}

			if (Step == 0)
			{
				if (Delta.x>Delta.y)
				{
					StepTrack.x++;
				}
				else
				{
					StepTrack.y++;
				}
			}
			else
			{
				if (Delta.y != 0)
				{
					float DeltaRatio = Delta.x / Delta.y;
					if (StepTrack.y != 0)
					{
						float CurRatio = StepTrack.x / StepTrack.y;
						if (CurRatio > DeltaRatio)
						{
							StepTrack.y++;
						}
						else
						{
							StepTrack.x++;
						}
					}
					else
					{
						StepTrack.y++;
					}
				}
				else
				{
					StepTrack.x++;
				}
			}

		}

	}
	return true;

}