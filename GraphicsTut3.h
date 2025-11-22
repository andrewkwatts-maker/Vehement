#include "Application.h"

#pragma once
class GraphicsTut3: public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;
	void generateGrid(unsigned int Rows, unsigned int Cols);

	unsigned int m_VAO; //Vertex Array Object
	unsigned int m_VBO; //Vertex Buffer Object
	unsigned int m_IBO; //Index Buffer Object

	unsigned int m_ProgramID;

	const char* VertexShadder;
	const char* FragmentShadder;

	GraphicsTut3();
	~GraphicsTut3();
};

