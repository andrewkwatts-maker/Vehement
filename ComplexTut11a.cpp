#include "ComplexTut11a.h"


ComplexTut11a::ComplexTut11a()
{
}


ComplexTut11a::~ComplexTut11a()
{
}


bool ComplexTut11a::update()
{

	if (Application::update())
	{
		handleNetworkMessages();
		return true;
	}
	else
	{
		return false;
	}

}
void ComplexTut11a::draw()
{
	Application::draw();
}


bool ComplexTut11a::startup()
{
	if (Application::startup())
	{
		handleNetworkConnection();
		return true;
	}
	else
	{
		return false;
	}
}

void ComplexTut11a::handleNetworkConnection()
{
	//initualize the Raknet peer interface first;
	m_pPeerInterface = RakNet::RakPeerInterface::GetInstance();
	initualizeClientConnection();
}
void ComplexTut11a::initualizeClientConnection()
{
	RakNet::SocketDescriptor sd;

	m_pPeerInterface->Startup(1, &sd,1);
	printf("Connection to server at: ");
	printf(IP);
	printf("\n");

	RakNet::ConnectionAttemptResult res = m_pPeerInterface->Connect(IP, PORT, nullptr, 0);

	if (res != RakNet::CONNECTION_ATTEMPT_STARTED)
	{
		printf("unable to start connection, Error Number: %i \n", res);
	}

}
void ComplexTut11a::handleNetworkMessages()
{
	RakNet::Packet* Packet;

	for (Packet = m_pPeerInterface->Receive(); Packet; m_pPeerInterface->DeallocatePacket(Packet), Packet = m_pPeerInterface->Receive())
	{
		switch (Packet->data[0])
		{
		case ID_REMOTE_DISCONNECTION_NOTIFICATION:
			printf("another client has disconected. \n");
			break;
		case ID_REMOTE_CONNECTION_LOST:
			printf("another client has lost the connection. \n");
			break;
		case ID_REMOTE_NEW_INCOMING_CONNECTION:
			printf("Another client has connected. \ n");
			break;
		case ID_CONNECTION_REQUEST_ACCEPTED:
			printf("our connection request has been accepted. \n");
			break;
		case ID_NO_FREE_INCOMING_CONNECTIONS:
			printf("the server is full.\n");
			break;
		case ID_DISCONNECTION_NOTIFICATION:
			printf("we have been disconnected. \n");
			break;
		case ID_CONNECTION_LOST:
			printf("ConnectionLost. \n");
			break;
		case ID_SERVER_TEXT_MESSAGE:
		{
			RakNet::BitStream bsIn(Packet->data, Packet->length, false);
			bsIn.IgnoreBytes(sizeof(RakNet::MessageID));

			RakNet::RakString str;

			bsIn.Read(str);
			printf(str.C_String());
			printf("\n");

			break;
		}
		default:
			printf("recieved connection with unknown id: %i \n", Packet->data[0]);
			break;
		}
	}
}