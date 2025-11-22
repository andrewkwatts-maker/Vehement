#pragma once
#include "Application.h"
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"

//Client That Sends Data to Server;
class ComplexTut11a :
	public Application
{
public:
	enum GameMessages
	{
		ID_SERVER_TEXT_MESSAGE = ID_USER_PACKET_ENUM + 1
	};

	bool update() override;
	void draw() override;
	bool startup() override;

	void handleNetworkConnection();
	void initualizeClientConnection();
	void handleNetworkMessages();

	RakNet::RakPeerInterface* m_pPeerInterface;

	const char* IP = "127.0.0.1";
	const unsigned short PORT = 5456;

	ComplexTut11a();
	~ComplexTut11a();
};

