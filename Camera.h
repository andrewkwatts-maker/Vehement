#pragma once
#include <glm\glm.hpp>
#include "Application.h"

using glm::mat4;
using glm::vec4;
using glm::vec3;
using glm::vec2;
using glm::dvec2;

struct GLFWwindow;

class Camera
{
public:

	void SetPerspective(float FieldOfView, float AspectRatio, float Near, float Far); //Create Before Using;

	void SetupCamera(vec3 Pos, vec3 Target, vec3 Up); //does 2nd, 3rd and 4th calls in one.

	void FlyCamera(AppBasics* Basics);

	vec3 GetPos();
	vec3 GetDirVector();

	mat4 GetWorldTransform();
	mat4 GetProjectionView();
	mat4 GetView();
	mat4 GetProjection();
	vec3 VectorThroughScreenPoint(vec2 Point, vec2 ScreenSize);

	Camera();
	~Camera();

private:
	void SetLookingAt(vec3 To, vec3 Up);
	void SetPosition(vec3 Pos);
	void UpdateProjectionViewTransforms();

	mat4 m_View;
	mat4 m_ProjectionView;
	mat4 m_worldTransform;
	mat4 m_Projection;
	vec3 m_Facing;

	float Speed;
	float SpeedBase;

	glm::dvec2 MouseLoc;
};

