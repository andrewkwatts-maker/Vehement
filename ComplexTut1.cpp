#include "ComplexTut1.h"
#include <glfw/glfw3.h>
#include <glm\ext.hpp>
#include <glm/gtx/transform.hpp>

#define _USE_MATH_DEFINES
ComplexTut1::ComplexTut1()
{
	pi = 3.14159265359f;
	
	q0 = quat(1, 0, 0, 0);
	q1 = quat(glm::vec3(0, pi, 2 * pi));
	
	q2 = quat(m3);
	q2 = quat(glm::toQuat(m3));
	
	q3 = quat(m4);
	q3 = quat(glm::toQuat(m4));
	
	m4 = mat4(glm::toMat4(q3));
	m3 = mat3(glm::toMat4(q3));
	
	rot2 = glm::slerp(q2, q3, s);
	
	
	//Moving Platform;
	m_positions[0] = glm::vec3(10, 5, 10);
	m_positions[1] = glm::vec3(-10, 0, 10);
	m_positions[2] = glm::vec3(-10, 5, -10);
	m_positions[3] = glm::vec3(10, 0, -10);
	m_rotations[0] = glm::quat(glm::vec3(0, -1, 0));
	m_rotations[1] = glm::quat(glm::vec3(0, 1, 0));
	m_rotations[2] = glm::quat(glm::vec3(1, -1, 0));
	m_rotations[3] = glm::quat(glm::vec3(1, 1, 0));


}

bool ComplexTut1::update()
{
	s = 3*(glm::cos((float)glfwGetTime()/3)*0.5f + 0.5f);
	if (s < 1)
	{
		Position = (1.0f - s)*m_positions[0] + s*m_positions[1];
		Rotation = glm::slerp(m_rotations[0], m_rotations[1], s);
	}
	else if (s < 2)
	{
		Position = (1.0f - (s-1))*m_positions[1] + (s-1)*m_positions[2];
		Rotation = glm::slerp(m_rotations[1], m_rotations[2], s-1);
	}
	else
	{
		Position = (1.0f - (s - 2))*m_positions[2] + (s-2)*m_positions[3];
		Rotation = glm::slerp(m_rotations[2], m_rotations[3], s - 2);
	}



	
	MatrixObject = glm::translate(Position)*glm::toMat4(Rotation);

	return Application::update();

};
void ComplexTut1::draw()
{
	Gizmos::addTransform(MatrixObject);
	Gizmos::addAABBFilled(Position, glm::vec3(0.5f), glm::vec4(1, 0, 0, 1), &MatrixObject);

	Application::draw();
};

ComplexTut1::~ComplexTut1()
{
}
