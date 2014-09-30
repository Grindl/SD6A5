#pragma once
#ifndef include_USER
#define include_USER

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "Entity.hpp"
#include "CS6Packet.hpp"


enum USER_TYPE{USER_LOCAL, USER_REMOTE};

class User
{
public:
	User();

	Entity m_unit;
	USER_TYPE m_userType;
	bool m_isInGame;
	double m_lastReceivedPacketTime;
	unsigned int m_lastSentPacketNum;
	unsigned int m_lastReceivedPacketNum;
	int m_score;
	unsigned char m_gameID;

	CS6Packet sendInput();
	void processUpdatePacket(CS6Packet newData);
	void update(float deltaSeconds);
	void render();
};

#endif