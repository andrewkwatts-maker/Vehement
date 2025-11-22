#include "Camera.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glfw/glfw3.h>
# define M_PI           3.14159265358979323846  /* pi */

//myIncludes;
#include "Camera.h"
#include "Clock.h"
#include "Inputs.h"

Camera::Camera()
{
	MouseLoc = dvec2(0);
	m_worldTransform = mat4();
	SpeedBase = 7.f;
	Speed = SpeedBase;
}

void Camera::SetPerspective(float FieldOfView, float AspectRatio, float Near, float Far)
{
	m_Projection = glm::perspective(FieldOfView, AspectRatio, Near, Far);
}

void Camera::SetLookingAt(vec3 To, vec3 Up)\
{
	m_View = glm::lookAt(GetPos(), To, Up); //GetPos() = vec3 of camera location; or otherwise vec3(m_worldTransform[3]);
	m_worldTransform = glm::inverse(m_View);
	m_Facing = glm::normalize(To - GetPos());

}

void Camera::SetPosition(vec3 Pos)
{
	m_worldTransform[3] = vec4(Pos, 1);
}

void Camera::SetupCamera(vec3 Pos, vec3 Target, vec3 Up)
{
	SetPosition(Pos);
	SetLookingAt(Target, Up);
	UpdateProjectionViewTransforms();
}

void Camera::FlyCamera(AppBasics* Basics)
{
	bool IsKeyDown = false;
	vec4 WorldPos = vec4(GetPos(),1);
	vec2 XYFacing = vec2(m_Facing.x, m_Facing.z);
	if (Basics->AppInputs->IsKeyDown(GLFW_KEY_W))
	{
		IsKeyDown = true;
		WorldPos += vec4(glm::normalize(m_Facing), 0)*Speed*(float)Basics->AppClock->GetDelta();
	}
	if (Basics->AppInputs->IsKeyDown(GLFW_KEY_S))
	{
		IsKeyDown = true;
		WorldPos -= vec4(glm::normalize(m_Facing), 0)*Speed*(float)Basics->AppClock->GetDelta();
	}
	if (Basics->AppInputs->IsKeyDown(GLFW_KEY_A))
	{
		IsKeyDown = true;
		vec2 LeftDirection = vec2(XYFacing.x * cos(-M_PI / 2.f) - XYFacing.y * sin(-M_PI / 2.f), XYFacing.y * cos(-M_PI / 2.f) + XYFacing.x * sin(-M_PI / 2.f));
		WorldPos += vec4(glm::normalize(LeftDirection).x, 0, glm::normalize(LeftDirection).y, 0)*Speed*(float)Basics->AppClock->GetDelta();
	}
	if (Basics->AppInputs->IsKeyDown(GLFW_KEY_D))
	{
		IsKeyDown = true;
		vec2 RightDirection = vec2(XYFacing.x * cos(M_PI / 2.f) - XYFacing.y * sin(M_PI / 2.f), XYFacing.y * cos(M_PI / 2.f) + XYFacing.x * sin(M_PI / 2.f));
		WorldPos += vec4(glm::normalize(RightDirection).x, 0, glm::normalize(RightDirection).y, 0)*Speed*(float)Basics->AppClock->GetDelta();
	}

	if (IsKeyDown == true)
	{
		Speed *= 1 * (1 + Basics->AppClock->GetDelta() / 5.f);
		Speed = SpeedBase;
	}
	else
	{
		Speed = SpeedBase;
	}

	m_worldTransform[3] = WorldPos;
	UpdateProjectionViewTransforms();

	dvec2 LastMousePos = MouseLoc;
	glfwGetCursorPos(Basics->window, &MouseLoc.x, &MouseLoc.y);
	dvec2 DeltaMouse = LastMousePos - MouseLoc;


	if (glfwGetMouseButton(Basics->window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
	{
		float AngleX = -DeltaMouse.x / 100.0f;

		vec2 NewXY = vec2(XYFacing.x * cos(AngleX) - XYFacing.y * sin(AngleX), XYFacing.y * cos(AngleX) + XYFacing.x * sin(AngleX));
		m_Facing = glm::normalize(vec3(NewXY.x, m_Facing.y +DeltaMouse.y/200.f, NewXY.y));
	

		SetupCamera(GetPos(), m_Facing + GetPos(), vec3(0, 1, 0));
		vec3 Facein = vec3(m_worldTransform[2]);
		bool lol = false;

	}
}

vec3 Camera::GetPos()
{
	return vec3(m_worldTransform[3]);
};
vec3 Camera::GetDirVector()
{
	return m_Facing;
}

mat4 Camera::GetWorldTransform()
{
	return m_worldTransform;
}

mat4 Camera::GetView()
{
	return m_View;
}

mat4 Camera::GetProjection()
{
	return m_Projection;
}

mat4 Camera::GetProjectionView()
{
	return m_ProjectionView;
}

void Camera::UpdateProjectionViewTransforms()
{
	m_View = glm::inverse(m_worldTransform);
	m_ProjectionView = m_Projection*m_View;
}

glm::vec3 Camera::VectorThroughScreenPoint(vec2 Point,vec2 ScreenSize)
{
	double x, y;
	int w, h;

	w = ScreenSize.x;
	h = ScreenSize.y;

	glm::vec4 screen = glm::vec4(2.0f*(Point.x / w) - 1.0f, 1.0f - (2.0f*(Point.y / h)), 1, 1);
	glm::vec4 pos = glm::inverse(GetProjectionView())*screen;

	pos.w = 1.0f / pos.w;
	pos.x *= pos.w;
	pos.y *= pos.w;
	pos.z *= pos.w;

	return glm::normalize(GetPos() - vec3(pos));
}


Camera::~Camera()
{
}
