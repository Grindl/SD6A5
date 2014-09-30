#pragma once
#ifndef include_GAMEROOM
#define include_GAMEROOM

#include <vector>
#include "Primitives/Color3b.hpp"
#include "RemoteUDPClient.hpp"

class GameRoom
{
public:
	GameRoom();

	unsigned char m_gameID;
	std::vector<RemoteUDPClient> m_clients;
	bool m_gameOver;
	Color3b m_itColor;
	double m_gameStartTime;
	bool m_gameDead;
	RemoteUDPClient m_host;

	void initGame();
	void processPacket(CS6Packet inPacket, RemoteUDPClient currentClient);
	void generateAndSendUpdates();
	bool isGameDead();
};

#endif