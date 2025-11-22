#pragma once
#include "Application.h"
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"

enum GameMessages
{
	ID_SERVER_TEXT_MESSAGE = ID_USER_PACKET_ENUM + 1
};

//Server that Listens for/to Connections
class ComplexTut11 :
	public Application
{
public:
	bool update() override;
	void draw() override;
	bool startup() override;

	void SendPing(RakNet::RakPeerInterface* pPeerInterface);

	const unsigned short PORT = 5456;
	RakNet::RakPeerInterface* pPeerInterface = nullptr;
	RakNet::Packet* packet = nullptr;
	float Timer = 0;
	ComplexTut11();
	~ComplexTut11();
};

