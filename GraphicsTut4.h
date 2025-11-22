#pragma once
#include "Application.h"
#include "aie\FBXFile.h"
class GraphicsTut4:public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	void createOpenGLBuffers(FBXFile* fbx);
	void cleanupOpenGLBuffers(FBXFile* fbx);

	FBXFile* m_fbx;
	unsigned int m_Program;

	const char* vsSource;
	const char* fsSource;

	unsigned int VertexShader;
	unsigned int FragmentShader;

	GraphicsTut4();
	~GraphicsTut4();
};

