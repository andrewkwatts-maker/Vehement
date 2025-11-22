#include "ANIM.h"

using glm::vec3;
using glm::quat;

ANIM_Sequence::ANIM_Sequence()
{
	Time = 0;
	TotalTime = 0;
	TimeWarp = 1;
	FrameCurrently = 0;
}

glm::mat4 ANIM_Sequence::GetFrame()
{
	vec3 Position;
	quat Rotation;

	float LerpQuantity = (Time - Keyframe_List[FrameCurrently].AccumulatedAnimationTime) / Keyframe_List[FrameCurrently].Segment_Time;

	if (Keyframe_List.size() > 1)
	{
		if (FrameCurrently == Keyframe_List.size() - 1)
		{
			Position = (1.0f - LerpQuantity)*Keyframe_List[FrameCurrently].Position + LerpQuantity*Keyframe_List[0].Position;
			Rotation = glm::slerp(Keyframe_List[FrameCurrently].Rotation, Keyframe_List[0].Rotation, LerpQuantity);
		}
		else
		{
			Position = (1.0f - LerpQuantity)*Keyframe_List[FrameCurrently].Position + LerpQuantity*Keyframe_List[FrameCurrently + 1].Position;
			Rotation = glm::slerp(Keyframe_List[FrameCurrently].Rotation, Keyframe_List[FrameCurrently + 1].Rotation, LerpQuantity);
		}
	}
	else if (Keyframe_List.size() == 1)
	{
		FrameCurrently = 0;
		Position = Keyframe_List[FrameCurrently].Position;
		Rotation = Keyframe_List[FrameCurrently].Rotation;
	}
	else
	{
		Position = vec3(0, 0, 0);
		Rotation = quat(0, 0, 0, 0);
	}

	return glm::translate(Position)*glm::toMat4(Rotation);

};

bool ANIM_Sequence::Update(float DeltaTime)
{
	Time += DeltaTime*TimeWarp;

	if (Keyframe_List.size() > 0)
	{
		while (Time >= Keyframe_List[FrameCurrently].AccumulatedAnimationTime + Keyframe_List[FrameCurrently].Segment_Time)
		{
			FrameCurrently++;
			if (FrameCurrently >= Keyframe_List.size())
			{
				Time -= TotalTime;
				FrameCurrently = 0;
			}
		}
		return true;
	}
	else
	{
		Time = 0;
		return false;
	}

};

void ANIM_Sequence::AddFrame(ANIM_Keyframe Frame)
{
	Frame.AccumulatedAnimationTime = TotalTime;
	Keyframe_List.push_back(Frame);
	TotalTime += Frame.Segment_Time;
};

ANIM_Frame::ANIM_Frame(vec3 P, quat R)
{
	Position = P;
	Rotation = R;
}

ANIM_Frame::~ANIM_Frame()
{

};



//template <typename T> INF_LOOP_RESULT<T>::INF_LOOP_RESULT()
//{
//
//};
//template <typename T> INF_LOOP_RESULT<T>::~INF_LOOP_RESULT()
//{
//
//};


//template <typename T> ANIM_INF_LOOP<T>::ANIM_INF_LOOP()
//{
//	EarliestPos = 1.0f;
//}

ANIM_INF_LOOP::ANIM_INF_LOOP()
{

}

//template <typename T>
//ANIM_INF_LOOP<T>::~ANIM_INF_LOOP()
//{
//	EarliestPos = 1.0f;
//}

ANIM_INF_LOOP::~ANIM_INF_LOOP()
{

}

//template <typename T> void ANIM_INF_LOOP<T>::AddAsset(T DataAsset, float Pos)
//{
//	Pos = remainderf(Pos, 1.0f);
//	if (Pos < EarliestPos)
//	{
//		EarliestPos = Pos;
//	}
//
//
//	if (AssetArray.size() == 0)
//	{
//		AssetArray.push_back(DataAsset);
//		Positions.push_back(Pos);
//	}
//	else
//	{
//		int CurrentID = 0;
//		bool FoundSpot = false;
//		CurrentID++;
//		while (CurrentID < AssetArray.size() && FoundSpot == false)
//		{
//			if (Pos < Positions[CurrentID])
//			{
//				AssetArray.emplace(AssetArray.begin() +CurrentID, DataAsset);
//				Positions.emplace(Positions.begin() + CurrentID, Pos);
//				FoundSpot = true;
//			}
//			else
//			{
//				CurrentID++;
//			}
//		}
//		if (FoundSpot == false)
//		{
//			AssetArray.emplace_back(DataAsset);
//			Positions.emplace_back(Pos);
//		}
//
//	}
//
//}


void ANIM_INF_LOOP::AddAsset(WheelType DataAsset, float Pos)
{
	Pos = fmod(Pos, 1); //remainder divided by 1

	if (AssetArray.size() == 0)
	{
		AssetArray.push_back(DataAsset);
		Positions.push_back(Pos);
	}
	else
	{
		int CurrentID = 0;
		bool FoundSpot = false;
		CurrentID++;
		while (CurrentID < AssetArray.size() && FoundSpot == false)
		{
			if (Pos < Positions[CurrentID])
			{
				AssetArray.emplace(AssetArray.begin() + CurrentID, DataAsset);
				Positions.emplace(Positions.begin() + CurrentID, Pos);
				FoundSpot = true;
			}
			else
			{
				CurrentID++;
			}
		}
		if (FoundSpot == false)
		{
			AssetArray.emplace_back(DataAsset);
			Positions.emplace_back(Pos);
		}

	}

}

//template <typename T>
//INF_LOOP_RESULT<T> ANIM_INF_LOOP<T>::GetValueAt(float Pos)
//{
//	float RequiredPos = remainderf(Pos - EarliestPos,1.0f);
//
//	int InspectionFrame = 0;
//	float InspectionPos = Positions[InspectionFrame] - EarliestPos;
//
//	while (RequiredPos>InspectionPos && InspectionFrame < AssetArray.size()-1)
//	{
//		InspectionFrame++;
//		if (InspectionFrame < AssetArray.size())
//			InspectionPos = Positions[InspectionFrame] - EarliestPos;
//	}
//	if (InspectionFrame >= AssetArray.size())
//	{
//		InspectionFrame = 0;
//	}
//
//
//	INF_LOOP_RESULT<T> Results;
//	Results.Next = AssetArray[InspectionFrame];
//	Results.Previous = AssetArray[GetLastFrame(InspectionFrame,AssetArray.size())];
//
//	float TotalDiffrence = remainderf(Positions[InspectionFrame] - Positions[GetLastFrame(InspectionFrame,AssetArray.size())], 1.0f);
//	float PartDiffrence = remainderf(Pos - Positions[GetLastFrame(InspectionFrame, AssetArray.size())], 1.0f);
//	float LerpRatio = PartDiffrence / TotalDiffrence;
//
//	Results.LERP_Ratio = LerpRatio;
//
//
//
//	return Results;
//
//
//}


INF_LOOP_RESULT ANIM_INF_LOOP::GetValueAt(float Pos)
{

	int InspectionFrame = 0;
	int SearchResult = -2; // -2 == not Found, -1 == Between First And Last (still to cross origin), 0 == Between First And Last (Crossed Orgin), 1-N Between N and N-1;  

	fmod(Pos,1);

	do
	{
		if (InspectionFrame == 0)
		{
			if (Pos < Positions[0] + 1  && Pos >= Positions[Positions.size() - 1]) //Before Origin
			{
				SearchResult = -1;
			}
			
			else if (Pos < Positions[0] && Pos >= Positions[Positions.size() - 1] - 1) //After Origin
			{
				SearchResult = 0;
			}
		}
		else
		{
			if (Pos < Positions[InspectionFrame] && Pos > Positions[InspectionFrame - 1])
			{
				SearchResult = InspectionFrame;
			}
		}
		InspectionFrame = GetNextFrame(InspectionFrame, Positions.size());
	} while (SearchResult == -2);


	INF_LOOP_RESULT Results;

	//Central LERP Values
	float TotalDiffrence;
	float PartDiffrence;
	if (SearchResult > 0)
	{
		Results.Next = AssetArray[SearchResult];
		Results.Previous = AssetArray[SearchResult -1];

		TotalDiffrence = Positions[SearchResult] - Positions[SearchResult - 1];
		PartDiffrence = Pos - Positions[SearchResult - 1];
	}
	else if (SearchResult == 0)
	{
		Results.Next = AssetArray[0];
		Results.Previous = AssetArray[AssetArray.size()-1];

		TotalDiffrence = (Positions[0] +1) - Positions[AssetArray.size() -1];
		PartDiffrence = (Pos + 1) - Positions[AssetArray.size() - 1];
	}
	else
	{
		Results.Next = AssetArray[0];
		Results.Previous = AssetArray[AssetArray.size() - 1];

		TotalDiffrence = (Positions[0] + 1) - Positions[AssetArray.size() - 1];
		PartDiffrence = Pos - Positions[AssetArray.size() - 1];
	}
	Results.LERP_Ratio = PartDiffrence / TotalDiffrence;
	Results.Current_Length = TotalDiffrence;

	//Previous Lerp Values;
	int FrameID1;
	int FrameID2;
	if (SearchResult <= 0)
	{
		FrameID1 = GetLastFrame(0, AssetArray.size());
		FrameID2 = GetLastFrame(GetLastFrame(0, AssetArray.size()), AssetArray.size());
	}
	else
	{
		FrameID1 = GetLastFrame(SearchResult, AssetArray.size());
		FrameID2 = GetLastFrame(GetLastFrame(SearchResult, AssetArray.size()), AssetArray.size());

	}
	Results.Previous2 = AssetArray[FrameID2];

	if (Positions[FrameID1] < Positions[FrameID2]) //We Are Going accross the Origin
	{
		Results.Prev_Length = Positions[FrameID1] + 1 - Positions[FrameID2];
	}
	else
	{
		Results.Prev_Length = Positions[FrameID1] - Positions[FrameID2];
	}

	//NextLerpValues;
	if (SearchResult <= 0)
	{
		FrameID1 = 0;
		FrameID2 = GetNextFrame(0, AssetArray.size());
	}
	else
	{
		FrameID1 = SearchResult;
		FrameID2 = GetNextFrame(SearchResult, AssetArray.size());
	}
	Results.Next2 = AssetArray[FrameID2];

	if (Positions[FrameID1] < Positions[FrameID2]) 
	{
		Results.Next_Length = Positions[FrameID1] - Positions[FrameID2];
	}
	else //We Are Going accross the Origin
	{
		Results.Next_Length = Positions[FrameID1] + 1 - Positions[FrameID2];
	}


	return Results;
}


//emplate <typename T>
//nt ANIM_INF_LOOP<T>::GetNextFrame(int Current, int SizeVector)
//
//	if (Current >= SizeVector - 1)
//	{
//		return 0;
//	}
//	else
//	{
//		return Current + 1;
//	}
//

int ANIM_INF_LOOP::GetNextFrame(int Current, int SizeVector)
{
	if (Current >= SizeVector - 1)
	{
		return 0;
	}
	else
	{
		return Current + 1;
	}
}


//template <typename T>
//int ANIM_INF_LOOP<T>::GetLastFrame(int Current, int SizeVector)
//{
//	if (Current <= 0)
//	{
//		return SizeVector - 1;
//
//	}
//	else
//	{
//		return Current - 1;
//	}
//}


int ANIM_INF_LOOP::GetLastFrame(int Current, int SizeVector)
{
	if (Current <= 0)
	{
		return SizeVector - 1;

	}
	else
	{
		return Current - 1;
	}
}


//template <typename T> 
//INF_LOOP_RESULT<T> ANIM_Wheel<T>::Update(vec3 NewPos, float Radius)
//{
//	vec2 Delta = vec2(NewPos.x - LastPosition.x, NewPos.z - LastPosition.z);
//	float Circumference = 2 * M_PI*Radius;
//	CurrentFraction = remainderf(CurrentFraction + Delta.length() / Circumference,1.0f);
//	LastPosition = NewPos;
//
//	return GetValueAt(CurrentFraction);
//
//
//}


INF_LOOP_RESULT ANIM_Wheel::Update(vec3 NewPos, float Radius)
{
	vec2 Delta = vec2(NewPos.x - LastPosition.x, NewPos.z - LastPosition.z);
	float Circumference = 2 * M_PI*Radius;
	float Length = hypotf(Delta.x, Delta.y);
	CurrentFraction =fmod(CurrentFraction + Length / Circumference,1);
	LastPosition = NewPos;

	return GetValueAt(CurrentFraction);


}


//ExplicitImplementations
//template class INF_LOOP_RESULT < std::vector<ANIM_Frame> > ;
//template class INF_LOOP_RESULT <ANIM_Frame>;
