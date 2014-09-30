#include "User.hpp"
#include "Connection.hpp"
#include "Primitives/Color3b.hpp"
#include "Systems/Time Utility.hpp"

User::User()
{
	m_isInGame = false;
	initializeTimeUtility();
	m_lastSentPacketNum = 1;
	m_lastReceivedPacketNum = 0;
	m_score = 0;
	m_gameID = 0;
}

CS6Packet User::sendInput()
{
	if (m_userType == USER_REMOTE || !m_isInGame)
	{
		return CS6Packet();
	}
	else
	{
		CS6Packet outPacket = m_unit.packForSend();
		outPacket.gameID = m_gameID;
		//assign packet number
		m_lastSentPacketNum++;
		outPacket.packetNumber = m_lastSentPacketNum;
		g_serverConnection->sendPacket(outPacket);
		return outPacket;
	}
}

void User::processUpdatePacket(CS6Packet newData)
{
	m_lastReceivedPacketTime = getCurrentTimeSeconds();
	switch(newData.packetType)
	{
	case 0:
		{
			//BAD
			break;
		}
	case TYPE_Update:
		{
			//if this packet is applicable for processing
			if (m_lastReceivedPacketNum < newData.packetNumber)
			{

				Vector2f nextTarget = Vector2f(newData.data.updated.xPosition, newData.data.updated.yPosition);
				//prepare the unit for autonomous movement
				m_unit.m_deadReckoningVelocity = nextTarget - m_unit.m_target;
				m_unit.m_deadReckoningVelocity = m_unit.m_deadReckoningVelocity.normalized();
				//if (!(m_unit.m_deadReckoningVelocity == Vector2f(0,0)))
				//{
					m_unit.m_hasReachedCurrentTarget = false;
				//}
				m_unit.m_target = nextTarget;
				m_unit.m_velocity.x = newData.data.updated.xVelocity;
				m_unit.m_velocity.y = newData.data.updated.yVelocity;
				m_unit.m_orientationDegrees = newData.data.updated.yawDegrees;
				Color3b tempColor;
				memcpy(&tempColor, newData.playerColorAndID, sizeof(tempColor));
				m_unit.m_color = Color4f(tempColor);
				//jump the player if the distance is too far
				if (m_unit.m_position == Vector2f(0,0) || m_unit.m_position.distanceSquared(m_unit.m_target) > 400)
				{
					m_unit.m_position.x = newData.data.updated.xPosition;
					m_unit.m_position.y = newData.data.updated.yPosition;

				}
				//and decide which packet should be processed next
				m_lastReceivedPacketNum = newData.packetNumber;
			}
			break;
		}
	case TYPE_Victory:
		{
			m_score++;
			break;
		}
	}
}

void User::update(float deltaSeconds)
{
	m_unit.update(deltaSeconds);
	sendInput();
}

void User::render()
{
	m_unit.render();
}