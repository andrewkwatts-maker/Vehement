#include "ComplexTut11.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Clock.h"
ComplexTut11::ComplexTut11()
{
}


ComplexTut11::~ComplexTut11()
{
}


void Send_Ping(RakNet::RakPeerInterface* pPeerInterface)
{
	RakNet::BitStream bs;
	bs.Write((RakNet::MessageID)GameMessages::ID_SERVER_TEXT_MESSAGE);
	bs.Write("Ping!");
	
	pPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

bool ComplexTut11::update()
{
	
	if (Application::update())
	{
		for (packet = pPeerInterface->Receive(); packet; pPeerInterface->DeallocatePacket(packet), packet = pPeerInterface->Receive())
		{
			switch (packet->data[0])
			{
			case ID_NEW_INCOMING_CONNECTION:
				std::cout << "A Connection is incomming. \n";
				break;
			case ID_DISCONNECTION_NOTIFICATION:
				std::cout << "A client has disconnected. \n";
			case ID_CONNECTION_LOST:
				std::cout << "A client lost the connection. \n";
			default:
				std::cout << "recieved a message with unknown id:" << packet->data[0];
				std::cout << "\n";
				break;
			}
		}

		Timer += appBasics->AppClock->GetDelta();
		if (Timer > 1)
		{
			Timer -= 1;
			Send_Ping(pPeerInterface);
		}

		return true;
	}
	else
	{
		return false;
	}

}
void ComplexTut11::draw()
{
	Application::draw();
}


bool ComplexTut11::startup()
{
	if (Application::startup())
	{
		std::cout << "starting up the Server" << std::endl;

		//Initualize the Peer Interface
		pPeerInterface = RakNet::RakPeerInterface::GetInstance();

		//Create A Socket Desctiptor to discribe this connection
		RakNet::SocketDescriptor sd(PORT, 0);

		//Now call Startup - max of 32 Connections, on the assigned port
		pPeerInterface->Startup(32, &sd, 1);
		pPeerInterface->SetMaximumIncomingConnections(32);

		//std::thread PingThread(SendPing, pPeerInterface);
		
	

		return true;
	}
	else
	{
		return false;
	}
}