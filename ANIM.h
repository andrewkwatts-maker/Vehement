#pragma once
#include <aie\Gizmos.h>
#include <glm\glm.hpp>
#include <glm\gtc\quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm\ext.hpp>
#include <vector>



# define M_PI           3.14159265358979323846  /* pi */
using glm::vec3;
using glm::vec2;
using glm::quat;

class ANIM_Frame
{
public:
	ANIM_Frame(vec3 P, quat R);
	~ANIM_Frame();
	vec3 Position;
	quat Rotation;
};

typedef std::vector<ANIM_Frame> WheelType;

//template <typename T>
//struct INF_LOOP_RESULT
//{
//
//	T Next;
//	T Previous;
//	float LERP_Ratio;
//};

struct INF_LOOP_RESULT
{

	WheelType Next;
	WheelType Next2;
	WheelType Previous;
	WheelType Previous2;

	float LERP_Ratio;
	float Current_Length;
	float Prev_Length;
	float Next_Length;
};

//template <typename T>
//class ANIM_INF_LOOP //A Loop of Length 1, That Can Be Progressed Through and return the nearest 2 DataAssets to the point you want.
//{
//public:
//
//	ANIM_INF_LOOP();
//	~ANIM_INF_LOOP();
//
//	//Functions
//	void AddAsset(T DataAsset, float Pos); //Pos will be interpriated between 0 and 1;
//	INF_LOOP_RESULT<T> GetValueAt(float Pos);
//
//private:
//	//DataStorage
//	std::vector<T> AssetArray;
//	std::vector<float> Positions;
//
//	float EarliestPos; //Shifts the loop so the first frame sits at Pos 0;
//	int GetNextFrame(int Current, int SizeVector);
//	int GetLastFrame(int Current, int SizeVector);
//};

class ANIM_INF_LOOP //A Loop of Length 1, That Can Be Progressed Through and return the nearest 2 DataAssets to the point you want.
{
public:

	ANIM_INF_LOOP();
	~ANIM_INF_LOOP();

	//Functions
	void AddAsset(WheelType DataAsset, float Pos); //Pos will be interpriated between 0 and 1;
	INF_LOOP_RESULT GetValueAt(float Pos);

private:
	//DataStorage
	std::vector<WheelType> AssetArray;
	std::vector<float> Positions;

	int GetNextFrame(int Current, int SizeVector);
	int GetLastFrame(int Current, int SizeVector);
};

//template <typename T>
//class ANIM_Wheel : public ANIM_INF_LOOP <T>
//{
//public:
//	INF_LOOP_RESULT<T> Update(vec3 NewLocation, float Radius);
//private:
//	float PreviousFraction; //HowFarAroundTheCircleAreYou?
//	vec3 LastPosition;
//
//};

class ANIM_Wheel : public ANIM_INF_LOOP
{
public:
	INF_LOOP_RESULT Update(vec3 NewLocation, float Radius);

private:
	float CurrentFraction; //HowFarAroundTheCircleAreYou?
	vec3 LastPosition;

};

static glm::mat4 LerpResults(vec3 NextLoc, quat NextRot, vec3 PrevLoc, quat PrevRot, float Lerp)
{
	vec3 Position = (1.0f - Lerp)*PrevLoc + Lerp*NextLoc;
	quat Rotation = glm::slerp(PrevRot, NextRot, Lerp);
	return glm::translate(Position)*glm::toMat4(Rotation);
};

static glm::mat4 SmoothLerpResults(vec3 NextLoc, quat NextRot, vec3 Next2Loc, quat Next2Rot, vec3 PrevLoc, quat PrevRot, vec3 Prev2Loc, quat Prev2Rot, float Lerp)
{
	vec3 PrevPosition;
	quat PrevRotation;

	vec3 NextPosition;
	quat NextRotation;

	vec3 Position;
	quat Rotation;

	float PreviousLerp = 1 + Lerp;
	float FutureLerp = Lerp-1;

	//PreviousResult = 
	PrevPosition = (1.0f - PreviousLerp)*Prev2Loc + PreviousLerp*PrevLoc;
	PrevRotation = glm::slerp(Prev2Rot, PrevRot, PreviousLerp);

	//NextResult = 
	NextPosition = (1.0f - FutureLerp)*NextLoc + FutureLerp*Next2Loc;
	NextRotation = glm::slerp(NextRot, Next2Rot, FutureLerp);

	//Results
	Position = (1.0f - Lerp)*PrevPosition + Lerp*NextPosition;
	Rotation = glm::slerp(PrevRotation, NextRotation, Lerp);

	return glm::translate(Position)*glm::toMat4(Rotation);
};

static glm::mat4 CardinalLerpResults(vec3 NextLoc, quat NextRot, vec3 Next2Loc, quat Next2Rot, vec3 PrevLoc, quat PrevRot, vec3 Prev2Loc, quat Prev2Rot, float Multiplier, float Lerp)
{
	//DoesNotYetWork


	vec3 Position;
	quat Rotation;

	vec3 TangentPos_Prev = (PrevLoc - Prev2Loc)*Multiplier;
	quat TangentQua_Prev = (PrevRot*glm::inverse(Prev2Rot))*Multiplier;
	vec3 TangentPos_Next = (Next2Loc - NextLoc)*Multiplier;
	quat TangentQua_Next = (Next2Rot*glm::inverse(NextRot))*Multiplier;

	//vec3 TangentPos_Prev = (PrevLoc - Prev2Loc)*Multiplier;
	//quat TangentQua_Prev = (PrevRot*glm::inverse(Prev2Rot))*Multiplier;
	//vec3 TangentPos_Next = (NextLoc - PrevLoc)*Multiplier;
	//quat TangentQua_Next = (NextRot*glm::inverse(PrevRot))*Multiplier;

	float Lsq = Lerp*Lerp;
	float Lcb = Lsq*Lerp;

	float h00 = 2 * Lcb - 3 * Lsq + 1;
	float h01 = -2 * Lcb + 3 * Lsq;
	float h10 = Lcb - 2 * Lsq + Lerp;
	float h11 = Lcb - Lsq;

	Position = h00*PrevLoc + h10*TangentPos_Prev + h01*NextLoc + h11*TangentPos_Next;
	Rotation = h00*PrevRot + h10*TangentQua_Prev + h01*NextRot + h11*TangentQua_Next;

	return glm::translate(Position)*glm::toMat4(Rotation);
};



//=====================================================================
//Bellow Material Of Tutorails Only
//=====================================================================

struct ANIM_Keyframe //WillPhaseOut; - Used For ComplexTutorial1a only.
{
public:
	ANIM_Keyframe(vec3 P, quat R, float T)
	{
		Position = P;
		Rotation = R;
		Segment_Time = T;
	}

	vec3 Position;
	quat Rotation;
	float Segment_Time;
	float AccumulatedAnimationTime;

};

class ANIM_Sequence
{
public:
	ANIM_Sequence();

	float Time;
	float TotalTime;
	float TimeWarp;
	glm::mat4 GetFrame();
	bool Update(float DeltaTime);
	void AddFrame(ANIM_Keyframe Frame);

private:
	std::vector<ANIM_Keyframe> Keyframe_List;
	int FrameCurrently;
};

