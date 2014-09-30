#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>

#include "Primitives/Color3b.hpp"
#include "Systems/Time Utility.hpp"

#include "RemoteUDPClient.hpp"
#include "GameRoom.hpp"

#pragma comment(lib, "Ws2_32.lib")


struct sockaddr_in g_serverSockAddr;
SOCKET g_Socket;
std::map<int, GameRoom> g_allGames;

unsigned char g_nextGameNum;

void startServer(const char* port)
{
	WSAData myWSAData;
	int WSAResult;
	g_Socket = INVALID_SOCKET;
	u_long fionbioFlag = 1;

	g_serverSockAddr.sin_family = AF_INET;
	g_serverSockAddr.sin_port = htons((unsigned short)atoi(port));
	g_serverSockAddr.sin_addr.s_addr = INADDR_ANY;

	WSAResult = WSAStartup(MAKEWORD(2,2), &myWSAData);
	g_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	WSAResult = ioctlsocket(g_Socket, FIONBIO, &fionbioFlag);
	WSAResult = bind(g_Socket, (struct sockaddr*)&g_serverSockAddr, sizeof(g_serverSockAddr));

	g_nextGameNum = 1;
}

CS6Packet receive()
{
	int WSAResult;
	CS6Packet pk;
	pk.packetType = 0;
	RemoteUDPClient currentClient;
	int recvSize = sizeof(currentClient.m_sockAddr);

	WSAResult = recvfrom(g_Socket, (char*)&pk, sizeof(pk), 0, (sockaddr*)&(currentClient.m_sockAddr), &recvSize);

	if (WSAResult != -1)
	{
		if (g_allGames.find(pk.gameID) != g_allGames.end())
		{
			g_allGames[pk.gameID].processPacket(pk, currentClient);
		}
		else
		{
			if (pk.gameID == 0 && pk.packetType == TYPE_JoinGame)
			{
				//create a new game
				g_allGames[g_nextGameNum] = GameRoom();
				g_allGames[g_nextGameNum].m_gameID = g_nextGameNum;
				g_allGames[g_nextGameNum].m_host = currentClient;
				g_allGames[g_nextGameNum].m_clients.push_back(currentClient);
				
				//and send them an ack
				g_allGames[g_nextGameNum].processPacket(pk, currentClient);
				g_nextGameNum++;
			}
			else
			{
				//send them the lobby list, the requested game doesn't exist
				currentClient.sendLobbyList(&g_allGames);
			}			
		}
	}
	else
	{
		WSAResult = WSAGetLastError();
		int BREAKNOW = 0;
	}

	return pk;
}


int main()
{
	//startGame();
	startServer("8080");
	while(true)
	{
		//Receive and process packets while there are pending packets
		CS6Packet currentPacket;
		do 
		{
			currentPacket = receive();
		} while (currentPacket.packetType != 0);

		
		for (auto gamesIterator = g_allGames.begin(); gamesIterator != g_allGames.end(); gamesIterator++)
		{
			gamesIterator->second.generateAndSendUpdates();
			if (gamesIterator->second.isGameDead())
			{
				for (unsigned int playerIndex = 0; playerIndex < gamesIterator->second.m_clients.size(); playerIndex++)
				{
					gamesIterator->second.m_clients[playerIndex].sendLobbyList(&g_allGames);
					//potential bug: they'll see the lobby with the game they were kicked from still listed
				}
			}
		}
		Sleep(50);
	}
	return 0;
}