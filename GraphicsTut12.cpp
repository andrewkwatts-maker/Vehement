#include "GraphicsTut12.h"

#include "GL_Manager.h"
#include <loadgen/gl_core_4_4.h>
#include "Camera.h"

#include <glfw/glfw3.h> 
GraphicsTut12::GraphicsTut12()
{
}


GraphicsTut12::~GraphicsTut12()
{
}

bool GraphicsTut12::update()
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
void GraphicsTut12::draw()
{
	//Draw To FrameBuffer;
	OGL_Manager->BeginNewDrawTo(FrameBuffer, vec4(0.1f, 0.1f, 0.1f, 1));
	Gizmos::addSphere(vec3(0), 1, 4, 4, vec4(1));
	Application::draw();
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());

	//Draw to Main Screen
	OGL_Manager->BeginNewDrawTo(0, vec4(0, 0, 0, 1));

	OGL_Manager->UseShader(ShaderProgram);
	OGL_Manager->SetRenderTargetAsTexture(RenderTarget,0,"diffuse");
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->DrawCustomGeometry(Mirror,vec3(0,5,0));
	Gizmos::addSphere(vec3(0), 1, 4, 4, vec4(1));
	Application::draw();

	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());

}


bool GraphicsTut12::startup()
{
	if (Application::startup())
	{
		ShaderProgram = OGL_Manager->AddShaders("./Shaders/VS_Textured.vert", "./Shaders/FS_Textured.frag");

		FrameBuffer = OGL_Manager->GenNewFrameTarget(128,128, false);
		RenderTarget = OGL_Manager->GenNewRenderTarget(FrameBuffer, GL_RGBA8);

		vector<VertexBasicTextured> Vertexs;
		vector<unsigned int> Indexs;

		Vertexs.push_back({ -5, 0, -5, 1, 0, 0 });
		Vertexs.push_back({ 5, 0, -5, 1, 1, 0 });
		Vertexs.push_back({ 5, 10*9/16, -5, 1, 1, 1 });
		Vertexs.push_back({ -5, 10*9/16, -5, 1, 0, 1 });

		Indexs.push_back(0);
		Indexs.push_back(1);
		Indexs.push_back(2);
		Indexs.push_back(0);
		Indexs.push_back(2);
		Indexs.push_back(3);

		Mirror = OGL_Manager->AddCustomGeometry(Vertexs,Indexs);
		RunDrawBeginAndEnd = false;
		return true;
	}
	else
	{
		return false;
	}
}