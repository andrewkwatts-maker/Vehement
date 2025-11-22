#include "PATH_NodeGraph.h"
#include "PATH_Edge.h"
#include "PATH_Node.h"
#include <stack>
#include <queue>
#include <list>

#include <limits>
#include <cfloat>


PATH_NodeGraph::PATH_NodeGraph(void)
{
	SteppedSearch = false;
	Wait = false;
	SelectedNode = NULL;
}


PATH_NodeGraph::~PATH_NodeGraph(void)
{
}

void PATH_NodeGraph::BuildRandomNetwork(int Nodes)
{
	for (int i = 0;i<Nodes;++i)
	{
		PATH_Node* TMP = AddNode();
		TMP->x = rand() % 800 + 100;
		TMP->y=rand()%600+100;
		TMP->z = 0;
		TMP->SetNodeNumber(i);
	}
	for (int i = 0;i<Nodes;++i)
	{
		for (int Conections = 0;Conections<rand()%7;Conections++)
		{
			PATH_Edge STD;
			STD.Start = GetNodeList()[i];
			STD.Finish = GetNodeList()[rand()%Nodes];
			STD.m_fWeight = rand()%100;
			GetNodeList()[i]->AddEdge(STD);
		}
	}
};

void PATH_NodeGraph::BuildSquareNetwork(int Length)
{
	for (int j = 0;j<Length;++j)
	{
		for (int i = 0;i<Length;++i)
		{
			PATH_Node* TMP = AddNode();
			TMP->x = i * 94 + 70 + rand() % 47;
			TMP->y=j*94+70+rand()%47;
			TMP->z = 0;
			TMP->SetNodeNumber(i+j*Length);
		}
	}
	for (int j = 0;j<Length;++j)
	{
		for (int i = 0;i<Length;++i)
		{
			PATH_Edge STD;
			float EdgeValue = 15;
			if (i!=Length-1)
			{
				STD.Start = GetNodeList()[i+j*Length];
				STD.Finish = GetNodeList()[(i+1)+j*Length];
				STD.m_fWeight = GetLength(GetNodeList()[i+j*Length],GetNodeList()[(i+1)+j*Length]);
				GetNodeList()[i+j*Length]->AddEdge(STD);

				STD.Finish = GetNodeList()[i+j*Length];
				STD.Start = GetNodeList()[(i+1)+j*Length];
				STD.m_fWeight = GetLength(GetNodeList()[i+j*Length],GetNodeList()[(i+1)+j*Length]);
				GetNodeList()[(i+1)+j*Length]->AddEdge(STD);
			}
			if (j!=Length-1)
			{
				STD.Start = GetNodeList()[i+j*Length];
				STD.Finish = GetNodeList()[(i)+(j+1)*Length];
				STD.m_fWeight = GetLength(GetNodeList()[i+j*Length],GetNodeList()[i+(j+1)*Length]);
				GetNodeList()[i+j*Length]->AddEdge(STD);

				STD.Finish = GetNodeList()[i+j*Length];
				STD.Start = GetNodeList()[(i)+(j+1)*Length];
				STD.m_fWeight = GetLength(GetNodeList()[i+j*Length],GetNodeList()[i+(j+1)*Length]);
				GetNodeList()[i+(j+1)*Length]->AddEdge(STD);
			}
		}
	}
};


//void NodeGraph::CreateNetwork(vec3 Camera)
//{
//	float Yvalue = FW_dProgram_MouseY+Camera.y;
//	float Xvalue = FW_dProgram_MouseX+Camera.x;
//	//SetingValues
//	if (FW_MousePressed(GLFW_MOUSE_BUTTON_1) == true)
//	{
//		if ( SelectedNode == NULL)
//		{
//			Node* TMP = AddNode();
//			TMP->x = Xvalue;
//			TMP->y = Yvalue;
//		}
//		else
//		{
//			SelectedNode->SetLocation(Xvalue,Yvalue);
//			CalculateEdgeWeights(SelectedNode);
//		}
//	}
//
//	if (FW_MousePressed(GLFW_MOUSE_BUTTON_2) == true)
//	{
//		float DistanceValue = FLT_MAX;
//		Node* MinDistanceNode = NULL;
//
//		for (int NodeN = 0;NodeN<m_aNodeList.size();NodeN++)
//		{
//			float Distance = hypotf(m_aNodeList[NodeN]->x - Xvalue,m_aNodeList[NodeN]->y - Yvalue);
//			if (Distance<DistanceValue)
//			{
//				DistanceValue = Distance;
//				MinDistanceNode = m_aNodeList[NodeN];
//			}
//		}
//		if (SelectedNode !=NULL && MinDistanceNode !=NULL && SelectedNode != MinDistanceNode)
//		{
//			Edge STD;
//			STD.Start = SelectedNode;
//			STD.Finish = MinDistanceNode;
//			STD.m_fWeight = GetLength(SelectedNode,MinDistanceNode);
//			AddEdge(STD);
//			STD.Start = MinDistanceNode;
//			STD.Finish = SelectedNode;
//			AddEdge(STD);
//		}
//
//		if (SelectedNode == MinDistanceNode && SelectedNode !=NULL)
//		{
//			SelectedNode->SetVisited(false);
//			SelectedNode = NULL;
//			MinDistanceNode = NULL;
//		}
//
//		if (MinDistanceNode !=NULL)
//		{
//			if (SelectedNode!=NULL)
//			{
//				SelectedNode->SetVisited(false);
//			}
//			SelectedNode = MinDistanceNode;
//			SelectedNode->SetVisited(true);
//		}
//	}
//
//	if (FW_KeyPressed(GLFW_KEY_DELETE) == true)
//	{
//		if (SelectedNode !=NULL)
//		{
//			RemoveNode(SelectedNode);
//			SelectedNode = NULL;
//
//		}
//	}
//
//	if (FW_KeyPressed(GLFW_KEY_1) == true)
//	{
//		SaveNodeGraph();
//	}
//
//	if (FW_KeyPressed(GLFW_KEY_2) == true)
//	{
//		LoadNodeGraph();
//	}
//
//};


PATH_Node* PATH_NodeGraph::AddNode()
{
	PATH_Node* NewNode = new PATH_Node;
	m_aNodeList.push_back(NewNode);
	return NewNode;
};
vector<PATH_Node*> PATH_NodeGraph::GetNodeList()
{
	return m_aNodeList;
};

void PATH_NodeGraph::RemoveEdge(PATH_Edge Input)
{
	bool FoundNode = false;
	int i = 0;
	while (FoundNode == false && i < m_aNodeList.size())
	{
		if (m_aNodeList[i] == Input.Start)
		{
			FoundNode == true;
			--i;
		}
		++i;
	}
	if (FoundNode == true)
	{
		m_aNodeList[i]->RemoveEdge(Input.Finish);
	}
};
void PATH_NodeGraph::RemoveNode(PATH_Node* NodePointer)
{
	for (int i = 0;i<m_aNodeList.size();++i)
	{
		m_aNodeList[i]->RemoveEdge(NodePointer);
	}
	bool FoundNode = false;
	int i = 0;
	while (FoundNode == false && i < m_aNodeList.size())
	{
		if (m_aNodeList[i] == NodePointer)
		{
			FoundNode == true;
			m_aNodeList.erase(m_aNodeList.begin()+i);
		}
		++i;
	}

	delete NodePointer;

};


void PATH_NodeGraph::CalculateEdgeWeights(PATH_Node* NodePointer)
{
	for (int i = 0;i<m_aNodeList.size();++i)
	{
		for (int EdgeN = 0; EdgeN<m_aNodeList[i]->GetEdges().size();EdgeN++)
		{
			if (m_aNodeList[i]->GetEdges()[EdgeN].Start == NodePointer || m_aNodeList[i]->GetEdges()[EdgeN].Finish == NodePointer)
			{
				m_aNodeList[i]->GetEdges()[EdgeN].m_fWeight = GetLength(m_aNodeList[i]->GetEdges()[EdgeN].Start,m_aNodeList[i]->GetEdges()[EdgeN].Finish);
			}
		}
	}

};
void PATH_NodeGraph::AddEdge(PATH_Edge Input)
{
	if (Input.Start !=Input.Finish)
	{
		bool FoundNode = false;
		int i = 0;
		while (FoundNode == false && i < m_aNodeList.size())
		{
			if (m_aNodeList[i] == Input.Start)
			{
				FoundNode == true;
				m_aNodeList[i]->AddEdge(Input);
			}
			++i;
		}
	}
};

void PATH_NodeGraph::DrawShortestPath()
{
    for( int NodeN = 0;NodeN<GetNodeList().size();++NodeN)
	{
		GetNodeList()[NodeN]->SetVisited(false);
	}

	PATH_Node* Next = SearchFinish;
	while (Next != SearchStart)
	{
		Next->SetVisited(true);
		Next = Next->GetLastNode();
	}
	SearchStart->SetVisited(true);
};

void PATH_NodeGraph::ResetForNewSearch(PATH_Node* Start, PATH_Node* Finish, bool Stepped)
{
	Wait = Stepped;
	SteppedSearch = Stepped;
	float Max = FLT_MAX;
	for( int NodeN = 0;NodeN<GetNodeList().size();++NodeN)
	{
		GetNodeList()[NodeN]->SetGScore(Max);
		GetNodeList()[NodeN]->SetFScore(Max);
		GetNodeList()[NodeN]->SetVisited(false);
		GetNodeList()[NodeN]->SetLastNode(NULL);
	}

	Start->SetLastNode(Start);

	while (oNodeStack.size()>0)
	{
		oNodeStack.pop();
	}
	while (oNodeQueue.size()>0)
	{
		oNodeQueue.pop();
	}
	while (oNodeVector.size()>0)
	{
		oNodeVector.pop_back();
	}
	while (oNodeVector.size()>0)
	{
		oNodeVectorA.pop_back();
	}
	oNodeQueue.push(Start);
	oNodeStack.push(Start);
	oNodeVector.emplace_back(Start);
	oNodeVectorA.emplace_back(Start);

	SearchFinish = Finish;
	SearchStart = Start;
	Start->SetGScore(0);
	Start->SetFScore(GetHeuristic(Start));
};
bool PATH_NodeGraph::DepthSearch()
{

	//keep looping until the stack is empty. 
	//This will only happen once we've checked every node. 
	while (!oNodeStack.empty() && Wait == false)
	{
	    //the rest of the algorithm goes in here
		PATH_Node* pCurrent = oNodeStack.top();
		oNodeStack.pop();

		if ( pCurrent->GetVisited() == true ) 
		{
			continue;
		}

		pCurrent->SetVisited(true);

		if (pCurrent == SearchFinish) 
		{
		    return true;
		}
		for ( int i = 0 ; i < pCurrent->GetEdges().size() ; ++i )
		{
			oNodeStack.push(pCurrent->GetEdges()[i].Finish);
		}

		Wait = SteppedSearch;
	}
	
	//return false if we didn't find a_pEnd
	return false;
};
bool PATH_NodeGraph::BreadthSearch()
{
	//keep looping until the stack is empty. 
	//This will only happen once we've checked every node. 
	while (!oNodeQueue.empty() && Wait == false)
	{
	    //the rest of the algorithm goes in here
		PATH_Node* pCurrent = oNodeQueue.front();
		oNodeQueue.pop();

		if ( pCurrent->GetVisited() == true ) 
		{
			continue;
		}

		pCurrent->SetVisited(true);

		if (pCurrent == SearchFinish) 
		{
		    return true;
		}
		for ( int i = 0 ; i < pCurrent->GetEdges().size() ; ++i )
		{
			oNodeQueue.push(pCurrent->GetEdges()[i].Finish);
		}

		Wait = SteppedSearch;
	}
	
	//return false if we didn't find a_pEnd
	return false;
};

void SwapNode(std::vector<PATH_Node*> &Data, int PosA, int PosB)
{
	PATH_Node* Tmp;
	Tmp = Data[PosA];
	Data[PosA] = Data[PosB];
	Data[PosB] = Tmp;
}

void BubbleSort(std::vector<PATH_Node*> & data)
{
    int length = data.size();

    for (int i = 0; i < length; ++i)
    {
        bool swapped = false;
        for (int j = 0; j < length - (i+1); ++j)
        {
			if (data[j]->GetGScore() < data[j+1]->GetGScore())
            {
				SwapNode(data, j, j+1);
                swapped = true;
            }
        }

        if (!swapped) break;
    }
}

void PATH_NodeGraph::BubbleSortH(std::vector<PATH_Node*> & data)
{
    int length = data.size();

    for (int i = 0; i < length; ++i)
    {
        bool swapped = false;
        for (int j = 0; j < length - (i+1); ++j)
        {
			if (data[j]->GetFScore()< data[j+1]->GetFScore())
            {
				SwapNode(data, j, j+1);
                swapped = true;
            }
        }

        if (!swapped) break;
    }
}

float PATH_NodeGraph::GetHeuristic(PATH_Node* Input)
{
	float HeuristicMode = 2;
     return HeuristicMode*hypot(Input->x - SearchFinish->x,Input->y - SearchFinish->y);
}

float PATH_NodeGraph::GetLength(PATH_Node* Input1, PATH_Node* Input2)
{
     return hypot(Input1->x - Input2->y,Input1->y - Input2->y);
}

PATH_Node* PATH_NodeGraph::GetNearestNode(vec3 Input)
{
		float DistanceValue = FLT_MAX;
		PATH_Node* MinDistanceNode = NULL;

		for (int NodeN = 0;NodeN<m_aNodeList.size();NodeN++)
		{
			float Distance = hypotf(m_aNodeList[NodeN]->x - Input.x,m_aNodeList[NodeN]->y - Input.y);
			if (Distance<DistanceValue)
			{
				DistanceValue = Distance;
				MinDistanceNode = m_aNodeList[NodeN];
			}
		}
		return MinDistanceNode;
};

PATH_Node* PATH_NodeGraph::GetNearestNode(float X, float Y)
{
		float DistanceValue = FLT_MAX;
		PATH_Node* MinDistanceNode = NULL;

		for (int NodeN = 0;NodeN<m_aNodeList.size();NodeN++)
		{
			float Distance = hypotf(m_aNodeList[NodeN]->x - X,m_aNodeList[NodeN]->y - Y);
			if (Distance<DistanceValue)
			{
				DistanceValue = Distance;
				MinDistanceNode = m_aNodeList[NodeN];
			}
		}
		return MinDistanceNode;
};



bool PATH_NodeGraph::Dijkstras()
{
	//Set All GScores To Max, Previous Nodes to Null and SetVisited to False ;
	bool FoundEnd = false;

	//keep looping until the stack is empty. 
	//This will only happen once we've checked every node. 
	while (!oNodeVector.empty() && Wait == false)
	{
		//sort(oNodeVector.begin(),oNodeVector.end());
		BubbleSort(oNodeVector);

	    //the rest of the algorithm goes in here
		PATH_Node* pCurrent = oNodeVector.back();
        oNodeVector.erase(oNodeVector.end()-1);

		if ( pCurrent->GetVisited() == true ) 
		{
			continue;
		}

		float CurrentGScore = pCurrent->GetGScore();
		pCurrent->SetVisited(true);

		if (pCurrent == SearchFinish) 
		{
             FoundEnd = true;
		}

		for ( int i = 0 ; i < pCurrent->GetEdges().size() ; ++i )
		{
			float NewGScore = pCurrent->m_aEdges[i].m_fWeight + CurrentGScore;
			if ( NewGScore < pCurrent->m_aEdges[i].Finish->GetGScore() && pCurrent !=SearchFinish);
			{   
				pCurrent->GetEdges()[i].Finish->SetGScore(NewGScore);
				//pCurrent->GetEdges()[i].Finish->SetLastNode(pCurrent);
				bool NodeExists = false;
				PATH_Node* LookingFor = pCurrent->m_aEdges[i].Finish;
				for (int NodeN = 0;NodeN<oNodeVector.size();NodeN++)
				{
					if ( oNodeVector[NodeN] == LookingFor)
					{
						NodeExists = true;
					}
				}
				if (NodeExists == false)
				{
					oNodeVector.push_back(LookingFor);
					if (pCurrent->GetLastNode() != LookingFor && LookingFor->GetVisited() == false)
					{
						LookingFor->SetLastNode(pCurrent);
					}
				}

			}
		}
		Wait = SteppedSearch;
	}
	
	//return false if we didn't find a_pEnd
	return FoundEnd;
};


bool PATH_NodeGraph::A_star()
{
	//Set All GScores To Max, Previous Nodes to Null and SetVisited to False ;
	bool FoundEnd = false;


	while (!oNodeVectorA.empty() && Wait == false)
	{
		BubbleSortH(oNodeVectorA);
		PATH_Node* pCurrent = oNodeVectorA.back();
        oNodeVectorA.erase(oNodeVectorA.end()-1);
		pCurrent->SetVisited(true);
		if (pCurrent == SearchFinish) 
		{
			 FoundEnd = true;
			 return true;
		}
		else
		{
			FoundEnd = false;
		}

		for ( int i = 0 ; i < pCurrent->GetEdges().size() ; ++i )
		{
			PATH_Node* LookingFor = pCurrent->GetEdges()[i].Finish;
			if (LookingFor->GetVisited() == false)
			{
				float NewGScore = pCurrent->GetEdges()[i].m_fWeight + pCurrent->GetGScore();
				float NewFScore = NewGScore + GetHeuristic(pCurrent->GetEdges()[i].Finish);
				if ( NewFScore < LookingFor->GetFScore())
				{   
					LookingFor->SetLastNode(pCurrent);
					LookingFor->SetGScore(NewGScore);
					LookingFor->SetFScore(NewFScore);


					bool NodeExists = false;
					for (int NodeN = 0;NodeN<oNodeVectorA.size();NodeN++)
					{
						if ( oNodeVectorA[NodeN] == LookingFor)
						{
							NodeExists = true;
						}
					}
					if (NodeExists == false)
					{
						oNodeVectorA.push_back(LookingFor);
					}
				}
			}
		}
		Wait = SteppedSearch;
	}

	return FoundEnd;
};

//void NodeGraph::SaveNodeGraph()
//{
//	fstream GraphOutput("NodeGraphData2.dat",ios::out | ios::binary);
//	int NodesInGraph = m_aNodeList.size();
//	GraphOutput.write((char*)&NodesInGraph,sizeof(int));
//	for (int i = 0;i<NodesInGraph;i++)
//	{
//		//Node Info
//		LocVec2 Info = m_aNodeList[i]->GetLocation();
//		GraphOutput.write((char*)&Info,sizeof(LocVec2));
//
//		//Edges
//		int Edges = m_aNodeList[i]->GetEdges().size();
//		GraphOutput.write((char*)&Edges,sizeof(int));
//
//		for (int NE = 0;NE<Edges;NE++)
//		{
//			int NodeN = 0;
//			Node* EndOfEdge = m_aNodeList[i]->GetEdges()[NE].Finish;
//			for (int N = 0;N<NodesInGraph;N++)
//			{
//				if (m_aNodeList[N] == EndOfEdge)
//				{
//					NodeN = N;
//				}
//			}
//			GraphOutput.write((char*)&NodeN,sizeof(int));
//
//			float EdgeWeight = m_aNodeList[i]->GetEdges()[NE].m_fWeight;
//			GraphOutput.write((char*)&EdgeWeight,sizeof(float));
//
//		}
//
//	}
//	GraphOutput.close();
//};
//void NodeGraph::LoadNodeGraph()
//{
//	while(m_aNodeList.size() > 0)
//	{
//		m_aNodeList.pop_back();
//	}
//
//	fstream GraphInput("NodeGraphData.dat",ios::in | ios::binary);
//	int NodesInGraph;
//	GraphInput.read((char*)&NodesInGraph,sizeof(int));
//	for (int i = 0;i<NodesInGraph;i++)
//	{
//		//Node Info
//		m_aNodeList.push_back(new Node);
//	}
//
//
//	for (int i = 0;i<NodesInGraph;i++)
//	{
//		//Node Info
//		LocVec2 Info;
//		GraphInput.read((char*)&Info,sizeof(LocVec2));
//		m_aNodeList[i]->SetLocation(Info);
//
//
//		//Edges
//		int Edges;
//		GraphInput.read((char*)&Edges,sizeof(int));
//		for (int NE = 0;NE<Edges;NE++)
//		{
//			Edge STD;
//			m_aNodeList[i]->AddEdge(STD);
//		}
//
//
//		for (int NE = 0;NE<Edges;NE++)
//		{
//			int NodeN = 0;
//			GraphInput.read((char*)&NodeN,sizeof(int));
//			Node* EndOfEdge = m_aNodeList[NodeN];
//			m_aNodeList[i]->m_aEdges[NE].Finish = EndOfEdge;
//
//			float EdgeWeight;
//			GraphInput.read((char*)&EdgeWeight,sizeof(float));
//			m_aNodeList[i]->m_aEdges[NE].m_fWeight = EdgeWeight;
//			m_aNodeList[i]->m_aEdges[NE].Start = m_aNodeList[i];
//		}
//
//	}
//	GraphInput.close();
//
//	for (int i = 0;i<NodesInGraph;i++)
//	{
//		//Node Info
//		CalculateEdgeWeights(m_aNodeList[i]);
//	}
//
//};