#pragma once
#include "Application.h"
#include "aie\FBXFile.h"

class GraphicsTut5: public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	void createOpenGLBuffers();
	void cleanupOpenGLBuffers();

	unsigned int m_texture;
	unsigned int m_vao; //vertex array object
	unsigned int m_vbo; //vertex buffer object
	unsigned int m_ibo; //index buffer object

	FBXFile* m_fbx;
	unsigned int m_Program;

	const char* vsSource;
	const char* fsSource;

	unsigned int VertexShader;
	unsigned int FragmentShader;

	GraphicsTut5();
	~GraphicsTut5();
};

