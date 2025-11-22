#include "GraphicsTut1.h"
#include <glfw/glfw3.h>
#include <glm\ext.hpp>
#include <glm/gtx/transform.hpp>

using glm::quat;
using glm::mat4;
using glm::mat3;
using glm::vec3;
using glm::vec4;

GraphicsTut1::GraphicsTut1()
{

}

bool GraphicsTut1::update()
{
	return Application::update();
}

void GraphicsTut1::draw()
{
	RunSolarSystem();
	Application::draw();
}

void GraphicsTut1::RunSolarSystem()
{
	float pi = 3.14159265359f;

	vec3 SunLocation(0, 0, 0);
	float sunRadius = 2;
	vec3 EarthDisplacment(8, 0, 0);
	float earthRadius = 0.2f;
	vec3 MoonDisplacment(0.5f, 0, 0);
	float moonRadius = 0.05f;

	float EarthOrbitLerp = (float)((int)(1000*glfwGetTime())%32000)/16000;
	float MoonOrbitLerp = (float)((int)(1000 * glfwGetTime()) % 4000) / 2000;

	quat Earth_rotations[4];
	quat Moon_rotations[4];

	Earth_rotations[0] = quat(glm::vec3(0, 0, 0));
	Earth_rotations[1] = quat(glm::vec3(0, -pi, 0));
	Earth_rotations[2] = glm::slerp(Earth_rotations[0], Earth_rotations[1], EarthOrbitLerp);
	Earth_rotations[3] = glm::slerp(Earth_rotations[0], Earth_rotations[1], -EarthOrbitLerp);

	Moon_rotations[0] = quat(glm::vec3(0, 0, 0));
	Moon_rotations[1] = quat(glm::vec3(0, pi, 0));
	Moon_rotations[2] = glm::slerp(Moon_rotations[0], Moon_rotations[1], MoonOrbitLerp);
	Moon_rotations[3] = glm::slerp(Moon_rotations[0], Moon_rotations[1], -MoonOrbitLerp);

	mat4 EarthPositionM = glm::toMat4(Earth_rotations[2])*glm::translate(EarthDisplacment);
	EarthPosition = vec3(EarthPositionM*vec4(0,0,0,1));
	mat4 MoonPositionM = glm::translate(EarthPosition)*glm::toMat4(Moon_rotations[2])*glm::translate(MoonDisplacment);
	MoonPosition = vec3(MoonPositionM*vec4(0,0,0, 1));

	Gizmos::addSphere(SunLocation, sunRadius, 27, 27, vec4(0.5f, 0.5f, 0, 1), &glm::toMat4(Earth_rotations[3]));
	Gizmos::addSphere(EarthPosition, earthRadius, 7, 7, vec4(0.0f, 0.0f, 0.5f, 1), &glm::toMat4(Moon_rotations[3]));
	Gizmos::addSphere(MoonPosition, moonRadius, 3, 3, vec4(0.5f, 0.5f, 0.5f, 1), nullptr);






}

GraphicsTut1::~GraphicsTut1()
{
}
