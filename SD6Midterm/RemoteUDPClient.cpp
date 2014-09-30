#include <algorithm>
#include <math.h>
#include "Primitives/Color3b.hpp"
#include "Systems/Time Utility.hpp"
#include "GameRoom.hpp"
#include "RemoteUDPClient.hpp"

extern SOCKET g_Socket;

bool packetComparitor(CS6Packet lhs, CS6Packet rhs)
{
	return lhs.packetNumber < rhs.packetNumber;
}

RemoteUDPClient::RemoteUDPClient()
{
	m_isDeclaringVictory = false;
	m_lastSentPacketNum = 1;
	m_lastReceivedPacketNum = 0;
	initializeTimeUtility();
}

void RemoteUDPClient::sendAllPendingPackets()
{
	double sendTime = getCurrentTimeSeconds();
	//loop through all the packets and set the send time and order number
	for (unsigned int outPacketIndex = 0; outPacketIndex < m_pendingPacketsToSend.size(); outPacketIndex++)
	{
		m_pendingPacketsToSend[outPacketIndex].timestamp = sendTime;
		if (m_pendingPacketsToSend[outPacketIndex].packetNumber == 0)
		{
			m_lastSentPacketNum++;
			m_pendingPacketsToSend[outPacketIndex].packetNumber = m_lastSentPacketNum;
		}
	}

	//send the actual packets
	int WSAResult;
	for (unsigned int ii = 0; ii < m_pendingPacketsToSend.size(); ii++)
	{
		WSAResult = sendto(g_Socket, (char*)&m_pendingPacketsToSend[ii], sizeof(m_pendingPacketsToSend[ii]), 0, (const sockaddr*)&(m_sockAddr), sizeof(m_sockAddr));
	}

	m_pendingPacketsToSend.clear(); //possibly dangerous since sendto isn't blocking
}


const bool RemoteUDPClient::operator==(const RemoteUDPClient& rhs) const
{
	bool isEquivalent = true;
	if (m_sockAddr.sin_addr.S_un.S_addr != rhs.m_sockAddr.sin_addr.S_un.S_addr)
	{
		isEquivalent = false;
	}
	if (m_sockAddr.sin_port != rhs.m_sockAddr.sin_port)
	{
		isEquivalent = false;
	}
	//TODO: in the future, a unique user ID may be considered as a way to differentiate clients on the same IP and port,
	//though they may step over each other on the receiving end
	return isEquivalent;
}


void RemoteUDPClient::processUnprocessedPackets(GameRoom* currentRoom)
{
	//sort prior to processing
	std::sort(m_unprocessedPackets.begin(), m_unprocessedPackets.end(), packetComparitor);
	while(!m_unprocessedPackets.empty())
	{
		CS6Packet currentPacket = m_unprocessedPackets.back();
		m_unprocessedPackets.pop_back();

		switch(currentPacket.packetType)
		{
		case 0:
			{
				//do nothing, invalid packet
				break;
			}
		case TYPE_JoinGame:
			{
				//prepare them on this end
				m_unit.m_color = Color4f(((float)rand())/RAND_MAX, ((float)rand())/RAND_MAX, ((float)rand())/RAND_MAX, 1.f);
				m_unit.m_position = Vector2f (rand()%500, rand()%500);
				//if there is no it, then THEY'RE IT!
				if (currentRoom->m_itColor == Color3b(0,0,0))
				{
					currentRoom->m_itColor = Color3b(m_unit.m_color);
				}

				//send them a reset
				//and tell them who IT is
				CS6Packet resetPacket;
				resetPacket.packetType = TYPE_GameStart;
				resetPacket.gameID = currentRoom->m_gameID;
				Color3b cleanedColor = Color3b(m_unit.m_color);
				memcpy(resetPacket.playerColorAndID, &cleanedColor, sizeof(cleanedColor));
				memcpy(resetPacket.data.reset.playerColorAndID, &cleanedColor, sizeof(cleanedColor));
				Color3b itColor = currentRoom->m_itColor;
				memcpy(resetPacket.data.reset.itPlayerColorAndID, &itColor, sizeof(itColor));
				resetPacket.data.reset.playerXPosition = m_unit.m_position.x;
				resetPacket.data.reset.playerYPosition = m_unit.m_position.y;
				//increment and set packet num
				m_lastSentPacketNum++;
				resetPacket.packetNumber = m_lastSentPacketNum;
				//and make sure they get it
				m_pendingGuaranteedPackets.push_back(resetPacket);
				break;
			}
			//else remove it from the non-acked list
		case TYPE_Acknowledge:
			{
				for (unsigned int ackIndex = 0; ackIndex < m_pendingGuaranteedPackets.size(); ackIndex++)
				{
					if(currentPacket.data.acknowledged.packetNumber == m_pendingGuaranteedPackets[ackIndex].packetNumber)
					{
						m_pendingGuaranteedPackets.erase(m_pendingGuaranteedPackets.begin()+ackIndex);
						ackIndex--;
					}
				}
				break;
			}
		case TYPE_Update:
			{
				//if more recent than the last
				if (m_lastReceivedPacketNum < currentPacket.packetNumber)
				{
					//update the relevant player in our data
					Vector2f relativeVelocity = m_unit.m_position - Vector2f(currentPacket.data.updated.xPosition, currentPacket.data.updated.yPosition);
					m_unit.m_position.x = currentPacket.data.updated.xPosition;
					m_unit.m_position.y = currentPacket.data.updated.yPosition;
					m_unit.m_velocity.x = currentPacket.data.updated.xVelocity;
					m_unit.m_velocity.y = currentPacket.data.updated.yVelocity;
					m_unit.m_orientation = currentPacket.data.updated.yawDegrees;
					//mark as most recent
					m_lastReceivedPacketNum = currentPacket.packetNumber;
				}
				break;
			}
		case TYPE_Victory:
			{
				//DEPRECATED
				//send up the chain that the game is over
				m_isDeclaringVictory = true;
				//ack back
				break;
			}
		}
	}
}


bool RemoteUDPClient::hasTimedOut()
{
	double currentTime = getCurrentTimeSeconds();
	bool timedOut = currentTime > m_lastReceivedPacketTime + 10.f;
	return timedOut;
}

void RemoteUDPClient::sendLobbyList(std::map<int, GameRoom>* lobbyList)
{
	for (auto gamesIterator = lobbyList->begin(); gamesIterator != lobbyList->end(); gamesIterator++)
	{
		CS6Packet lobbyPacket;
		lobbyPacket.packetType = TYPE_GameListing;
		m_lastSentPacketNum++;
		lobbyPacket.packetNumber = m_lastSentPacketNum;
		lobbyPacket.gameID = gamesIterator->second.m_gameID;
		lobbyPacket.data.gameList.gameID = gamesIterator->second.m_gameID;
		lobbyPacket.data.gameList.numPlayers = gamesIterator->second.m_clients.size();


		sendto(g_Socket, (char*)&lobbyPacket, sizeof(lobbyPacket), 0, (const sockaddr*)&(m_sockAddr), sizeof(m_sockAddr));
	}
}