#pragma once
#include "VORO_MATHS.h"

#include <vector>

class VORO_CELL :public VORO_CELL_CALCULATOR
{
public:
	void Gen_GL_Buffers();
	void Delete_GL_Buffers();
	void Delete_LeaveSeed();

	void Draw();

	VORO_CELL(vec3 Loc, float scale,VoroType Type);
	~VORO_CELL();
private:
	unsigned int GenBuffer_FaceCount;
	unsigned int* GenBuffer_EdgeCount;
	bool HasGLBuffers;

	//OGL Buffer Data; - One for Each Face;
	unsigned int* VBO;
	unsigned int* VAO;
	unsigned int* IBO;
};

