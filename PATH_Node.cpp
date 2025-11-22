#include "PATH_Node.h"
#include "PATH_Edge.h"


PATH_Node::PATH_Node(void)
{
	SetVisited(false);
}

bool PATH_Node::operator < (PATH_Node* str)
{
	return (GetGScore() < str->GetGScore());
}



void PATH_Node::AddEdge(PATH_Edge Addition)
{
	bool FoundNode = false;
	int i = 0;
	while (FoundNode == false && i < m_aEdges.size())
	{
		if (m_aEdges[i].Finish == Addition.Finish && Addition.Finish!= NULL)
		{
			FoundNode == true;
			//m_aEdges[i] = Addition; //Replacment
		}
		++i;
	}
	if (FoundNode == false)
	{
		m_aEdges.push_back(Addition); //Addition
	}
};

void PATH_Node::RemoveEdge(PATH_Node* NodePointer)
{
	bool FoundNode = false;
	int i = 0;
	while (FoundNode == false && i < m_aEdges.size())
	{
		if (m_aEdges[i].Finish == NodePointer)
		{
			FoundNode == true;
			m_aEdges.erase(m_aEdges.begin() + i);
		}
		++i;
	}
};

vector<PATH_Edge> PATH_Node::GetEdges()
{
	return m_aEdges;
};

void PATH_Node::SetNodeNumber(int iNodeNumber)
{
	m_iNodeNumber = iNodeNumber;
}

int PATH_Node::GetNodeNumber()
{
	return m_iNodeNumber;
}


PATH_Node::~PATH_Node(void)
{
}
void PATH_Node::SetVisited(bool Value)
{
	m_bVisited = Value;
};
bool PATH_Node::GetVisited()
{
	return m_bVisited;
};

float PATH_Node::GetGScore()
{
	return m_fGscore;
};
void PATH_Node::SetGScore(float Input)
{
	m_fGscore = Input;
};

float PATH_Node::GetFScore()
{
	return m_fFscore;
};
void PATH_Node::SetFScore(float input)
{
	m_fFscore = input;
};

PATH_Node* PATH_Node::GetLastNode()
{
	return m_nLastNode;
};
void PATH_Node::SetLastNode(PATH_Node* Input)
{
	m_nLastNode = Input;
};