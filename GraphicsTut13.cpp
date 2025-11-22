#include "GraphicsTut13.h"
#include "GL_Manager.h"
#include "Camera.h"
#include <loadgen/gl_core_4_4.h>
GraphicsTut13::GraphicsTut13()
{
}


GraphicsTut13::~GraphicsTut13()
{
}


bool GraphicsTut13::update()
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
void GraphicsTut13::draw()
{
	//Draw To FrameBuffer;
	OGL_Manager->BeginNewDrawTo(FrameBuffer, vec4(0.0f, 0.0f, 0.0f, 1));
	Gizmos::addSphere(vec3(0), 1, 4, 4, vec4(1));
	Application::draw();
	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());

	//Draw to Main Screen
	OGL_Manager->BeginNewDrawTo(0, vec4(0, 0, 0, 1));

	OGL_Manager->UseShader(ShaderProgram);
	OGL_Manager->SetRenderTargetAsTexture(RenderTarget1, 0, "diffuse");
	OGL_Manager->PassInUniform("ProjectionView", appBasics->AppCamera->GetProjectionView());
	OGL_Manager->DrawCustomGeometry(Screen, vec3(0, 0, 0));
	Gizmos::addSphere(vec3(0), 1, 4, 4, vec4(1));
	Application::draw();

	OGL_Manager->EndDrawCall(appBasics->AppCamera->GetProjectionView());

}


bool GraphicsTut13::startup()
{
	if (Application::startup())
	{
		ShaderProgram = OGL_Manager->AddShaders("./Shaders/VS_Textured_NoCam.vert", "./Shaders/FS_Textured_NoCam.frag");

		FrameBuffer = OGL_Manager->GenNewFrameTarget(appBasics->ScreenSize.x, appBasics->ScreenSize.y,false);
		RenderTarget2 = OGL_Manager->GenNewRenderTarget(FrameBuffer, GL_DEPTH_COMPONENT);
		RenderTarget1 = OGL_Manager->GenNewRenderTarget(FrameBuffer, GL_RGBA8);
		
		
		Screen = OGL_Manager->AddFullscreenQuadGeometry(0, appBasics->ScreenSize);
		RunDrawBeginAndEnd = false;
		return true;
	}
	else
	{
		return false;
	}
}