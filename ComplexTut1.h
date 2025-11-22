#pragma once
#include "Application.h"
#include <glm\gtc\quaternion.hpp>
//#include <glm\gtx\quaternion.hpp>



using glm::quat;
using glm::mat4;
using glm::mat3;
using glm::vec3;

class ComplexTut1: public Application
{
public:
	float pi;
	quat q0;
	quat q1;
	quat q2;
	quat q3;
	
	quat rot2;
	
	mat4 m4;
	mat3 m3;
	
	//Moving Platform
	
	float s; //slerp value
	vec3 m_positions[4];
	quat m_rotations[4];
	
	vec3 Position;
	quat Rotation;
	mat4 MatrixObject;
	
	bool update() override;
	void draw() override;

	ComplexTut1();
	~ComplexTut1();
};

