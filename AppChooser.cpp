#include "AppChooser.h"

#include "ComplexTut1.h"
#include "ComplexTut1a.h"
#include "ComplexTut2.h"
#include "ComplexTut3.h"
#include "ComplexTut4.h"
#include "ComplexTut5.h"
#include "ComplexTut6.h"
#include "ComplexTut8.h"
#include "ComplexTut11.h"
#include "ComplexTut11a.h"


#include "GraphicsTut1.h"
#include "GraphicsTut3.h"
#include "GraphicsTut4.h"
#include "GraphicsTut5.h"
#include "GraphicsTut6.h"
#include "GraphicsTut6a.h"
#include "GraphicsTut7.h"
#include "GraphicsTut8.h"
#include "GraphicsTut9.h"
#include "GraphicsTut10.h"
#include "GraphicsTut11.h"
#include "GraphicsTut12.h"
#include "GraphicsTut13.h"
#include "GraphicsTut14.h"

#include "ANIM_ExampleApp1.h"
#include "VoronoiExample3.h"
#include "GraphicsAssigment.h"
#include "ComplexAssesment.h"
#include "IndustryShowcase.h"

AppChooser::AppChooser()
{
}

Application* AppChooser::GetApp(Apps APP_ID)
{
	switch (APP_ID)
	{
		//Empty 3d Applicaiton wGrid;
	case Blank:
		return new Application;

		//MyExamples
	case AnimationExample1:
		return new ANIM_ExampleApp1;
	case VoroExample3:
		return new VoronoiExample3;
	case GraphicsAssigment1:
		return new GraphicsAssigment;
	case ComplexAssigment1:
		return new ComplexAssesment;
	case IndustryShowcase1:
		return new IndustryShowcase;

		//ComplexTutorials
	case ComplexTutorial1:
		return new ComplexTut1;
	case ComplexTutorial1a:
		return new ComplexTut1a;
	case ComplexTutorial2:
		return new ComplexTut2;
	case ComplexTutorial3:
		return new ComplexTut3;
	case ComplexTutorial4:
		return new ComplexTut4;
	case ComplexTutorial5:
		return new ComplexTut5;
	case ComplexTutorial6:
		return new ComplexTut6;
	case ComplexTutorial8:
		return new ComplexTut8;
	case ComplexTutorial11:
		return new ComplexTut11;	
	case ComplexTutorial11a:
			return new ComplexTut11a;


		//GraphicsTutorials
	case GraphicsTutorial1:
		return new GraphicsTut1;
	case GraphicsTutorial3:
		return new GraphicsTut3;
	case GraphicsTutorial4:
		return new GraphicsTut4;
	case GraphicsTutorial5:
		return new GraphicsTut5;
	case GraphicsTutorial6:
		return new GraphicsTut6;
	case GraphicsTutorial6a:
		return new GraphicsTut6a;
	case GraphicsTutorial7:
		return new GraphicsTut7;
	case GraphicsTutorial8:
		return new GraphicsTut8;
	case GraphicsTutorial9:
		return new GraphicsTut9;
	case GraphicsTutorial10:
		return new GraphicsTut10;
	case GraphicsTutorial11:
		return new GraphicsTut11;
	case GraphicsTutorial12:
		return new GraphicsTut12;
	case GraphicsTutorial13:
		return new GraphicsTut13;
	case GraphicsTutorial14:
		return new GraphicsTut14;

		//Default
	default:
		return new Application;
		break;
	}
};


AppChooser::~AppChooser()
{
}
