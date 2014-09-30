#include "GameRoom.hpp"
#include "Systems/Time Utility.hpp"


GameRoom::GameRoom()
{
	initGame();
}

void GameRoom::initGame()
{
	initializeTimeUtility();
	m_gameOver = false;
	m_gameDead = false;
	m_itColor = Color3b(0,0,0);
	m_gameStartTime = getCurrentTimeSeconds();
}

void GameRoom::processPacket(CS6Packet inPacket, RemoteUDPClient currentClient)
{
	bool newClient = true;

	for (unsigned int ii = 0; ii < m_clients.size(); ii++)
	{
		if (currentClient == m_clients[ii])
		{			
			newClient = false;
			double currentTime = getCurrentTimeSeconds();
			m_clients[ii].m_lastReceivedPacketTime = currentTime;
			//only process packets that are from the current game
			if (currentTime > m_gameStartTime)
			{
				m_clients[ii].m_unprocessedPackets.push_back(inPacket);
			}
		}
	}
	if (newClient)
	{
		std::cout<<"New client connected\n";
		double currentTime = getCurrentTimeSeconds();
		currentClient.m_lastReceivedPacketTime = currentTime;
		currentClient.m_unprocessedPackets.push_back(inPacket);
		m_clients.push_back(currentClient);
	}
}

void GameRoom::generateAndSendUpdates()
{
	Vector2f itLocation = Vector2f(0,0);
	//Collect all gameplay packets we need to send to other players
	std::vector<CS6Packet> positionUpdatePackets;
	for (unsigned int i = 0; i < m_clients.size(); i++)
	{
		//check for timeout
		
		if (m_clients[i].hasTimedOut())
		{
			//also check to see if the person who timed out is the "owner"
			//if so, scrap the update packets and set m_gameDead to true
			if (m_clients[i] == m_host)
			{
				m_gameDead = true;
				positionUpdatePackets.clear();
			}
			m_clients.erase(m_clients.begin()+i);
			i--;
		}
		else
		{
			//else process all their pending packets and put their new position in the queue			
			m_clients[i].processUnprocessedPackets(this);
			if (!m_gameDead)
			{
				CS6Packet nextUpdatePacket = m_clients[i].m_unit.packForSend();
				nextUpdatePacket.gameID = m_gameID;
				positionUpdatePackets.push_back(nextUpdatePacket);
			}			

			if (Color3b(m_clients[i].m_unit.m_color) == m_itColor)
			{
				itLocation = m_clients[i].m_unit.m_position;
			}
		}
	}

	for (unsigned int victimIndex = 0; victimIndex < m_clients.size(); victimIndex++)
	{
		bool playersTouching = false;
		//loop through all players to see if there is a victory condition
		if (!(Color3b(m_clients[victimIndex].m_unit.m_color) == m_itColor))
		{
			if (m_clients[victimIndex].m_unit.m_position.distanceSquared(itLocation) < 100.f)
			{
				playersTouching = true;
			}
		}

		if (playersTouching && !m_gameOver)
		{
			std::cout<<"Game Over!\n";
			m_gameOver = true;
			//if so, put in a victory packet
			CS6Packet vicPacket;
			vicPacket.packetType = TYPE_Victory;
			vicPacket.gameID = m_gameID;
			vicPacket.packetNumber = 0;
			Color3b cleanedWinnerColor = Color3b(m_clients[victimIndex].m_unit.m_color);
			memcpy(vicPacket.playerColorAndID, &cleanedWinnerColor, sizeof(cleanedWinnerColor));
			memcpy(vicPacket.data.victorious.winningPlayerColorAndID, &cleanedWinnerColor, sizeof(cleanedWinnerColor));
			memcpy(vicPacket.data.victorious.taggedPlayerColorAndID, &m_itColor, sizeof(m_itColor));
			positionUpdatePackets.push_back(vicPacket);
			m_itColor = Color3b(m_clients[victimIndex].m_unit.m_color);
		}			
	}

	for (unsigned int i = 0; i < m_clients.size(); i++)
	{
		//Generate any relevant non-gameplay packets
		if (m_gameOver)
		{
			CS6Packet resetPkt;
			resetPkt.packetType = TYPE_GameStart;
			resetPkt.gameID = m_gameID;
			Color3b cleanedColor = Color3b(m_clients[i].m_unit.m_color);
			memcpy(resetPkt.playerColorAndID, &cleanedColor, sizeof(cleanedColor));
			memcpy(resetPkt.data.reset.playerColorAndID, &cleanedColor, sizeof(cleanedColor));
			memcpy(resetPkt.data.reset.itPlayerColorAndID, &m_itColor, sizeof(m_itColor));
			m_clients[i].m_unit.m_position = Vector2f (rand()%500, rand()%500);
			resetPkt.data.reset.playerXPosition = m_clients[i].m_unit.m_position.x;
			resetPkt.data.reset.playerYPosition = m_clients[i].m_unit.m_position.y;
			//give a packet number
			m_clients[i].m_lastSentPacketNum++;
			resetPkt.packetNumber = m_clients[i].m_lastSentPacketNum;
			//and add to the send list
			m_clients[i].m_pendingGuaranteedPackets.push_back(resetPkt);

		}
		//put all of the gameplay packets and non-acked guaranteed packets in to a vector to send to the player
		m_clients[i].m_pendingPacketsToSend.insert(m_clients[i].m_pendingPacketsToSend.end(), m_clients[i].m_pendingGuaranteedPackets.begin(), m_clients[i].m_pendingGuaranteedPackets.end());
		m_clients[i].m_pendingPacketsToSend.insert(m_clients[i].m_pendingPacketsToSend.end(), positionUpdatePackets.begin(), positionUpdatePackets.end());
		//and then send it to them
		m_clients[i].sendAllPendingPackets();
	}

	//if the game has ended, reset the game
	if(m_gameOver)
	{
		m_gameOver = false;
		m_gameStartTime = getCurrentTimeSeconds() + 2.0; //a two second wait between rounds
	}
}

bool GameRoom::isGameDead()
{
	return m_gameDead;
}