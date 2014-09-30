#include <algorithm>

#include "Rendering\OpenGLFunctions.hpp"
#include "Rendering\Texture.hpp"
#include "Debug Graphics\DebugGraphics.hpp"
#include "Systems\Player.hpp"
#include "Rendering\Skybox.hpp"
#include "Console\FontRenderer.hpp"
#include "Data.hpp"
#include "GameMain.hpp"
#include "Utility\XMLParser.hpp"
#include "Event System\EventSystemHelper.hpp"
#include "Entity.hpp"
#include "Connection.hpp"
#include "Primitives/Color3b.hpp"
#include "Utility/StringUtility.hpp"
#include "Console\CommandParser.hpp"
#include "Systems\Time Utility.hpp"

bool g_pulse = false;
Vector2f cursorPositionInCamera; 
Vector2f cursorPositionInWorld;
User g_localUser;
Connection* g_serverConnection;
std::vector<User*> g_users;
Color3b g_itColor;

bool ChangeServer(std::string addressAndPort)
{
	std::vector<std::string> serverAddrSplit = StringUtility::StringSplit(addressAndPort, ":", " ");
	g_serverConnection = new Connection(serverAddrSplit[0].c_str(), serverAddrSplit[1].c_str());
	CS6Packet loginPacket;
	loginPacket.packetType = TYPE_Acknowledge;
	loginPacket.packetNumber = 0;
	loginPacket.data.acknowledged.packetType = TYPE_Acknowledge;
	loginPacket.gameID = 0;
	g_serverConnection->sendPacket(loginPacket);
	return g_serverConnection != NULL;
}

bool ChangeColor(std::string rgbaColor)
{
	g_localUser.m_unit.m_color = Color4f(rgbaColor);
	return true;
}

bool JoinGame(std::string gameIDAsString)
{
	unsigned char parsedGameID = atoi(gameIDAsString.c_str());
	CS6Packet gameJoinPacket;
	gameJoinPacket.packetType = TYPE_JoinGame;
	gameJoinPacket.gameID = parsedGameID;
	gameJoinPacket.data.joining.gameID = parsedGameID;
	g_localUser.m_lastSentPacketNum++;
	gameJoinPacket.packetNumber = g_localUser.m_lastSentPacketNum;
	g_serverConnection->sendPacket(gameJoinPacket);

	return true;
}

bool packetComparitor(CS6Packet lhs, CS6Packet rhs)
{
	return lhs.packetNumber < rhs.packetNumber;
}


void Tag::mouseUpdate()
{
	//TODO this is all very WIN32, abstract this in to the IOHandler
	if(m_IOHandler.m_hasFocus)
	{
		LPPOINT cursorPositionOnScreen = new POINT;
		GetCursorPos(cursorPositionOnScreen);
		HWND localClientWindow = m_IOHandler.m_clientWindow;
		ScreenToClient(localClientWindow, cursorPositionOnScreen);
		Vector2f cursorPositionInClient = Vector2f(cursorPositionOnScreen->x, cursorPositionOnScreen->y);
		cursorPositionInClient.y = SCREEN_HEIGHT - cursorPositionInClient.y;
		cursorPositionInCamera = cursorPositionInClient * (m_worldCamera.m_cameraSizeInWorldUnits.x / m_worldCamera.m_screenDimensionsInPixels.x);
		cursorPositionInWorld = cursorPositionInCamera + Vector2f(m_worldCamera.m_position.x, m_worldCamera.m_position.y); // - (m_worldCamera.m_cameraSizeInWorldUnits * .5);
		//TODO figure out what I want to use the mouse for

	}
	glColor3f(1.0f,1.0f,1.0f);
}

Tag::Tag()
{
	
	m_renderer = Renderer();
	m_internalTime = 0.f;
	m_isQuitting = m_renderer.m_fatalError;
	m_console.m_log = ConsoleLog();
	m_worldCamera = Camera();

	m_displayConsole = false;

	//HACK test values
	m_console.m_log.appendLine("This is a test of the emergency broadcast system");
	m_console.m_log.appendLine("Do not be alarmed or concerned");
	m_console.m_log.appendLine("This is only a test");

	UnitTestXMLParser(".\\Data\\UnitTest.xml");
	unitTestEventSystem();
	g_serverConnection = new Connection("127.0.0.1", "8080");
	g_localUser = User();
	g_localUser.m_unit = Entity();
	g_localUser.m_unit.m_color = Color4f(0.2f, 1.0f, 0.2f, 1.f);
	g_localUser.m_userType = USER_LOCAL;
	g_localUser.m_unit.m_position = Vector2f(0,0);
	g_itColor = Color3b(0,0,0);
	CommandParser::RegisterCommand("connect", ChangeServer);
	CommandParser::RegisterCommand("color", ChangeColor);
	CommandParser::RegisterCommand("join", JoinGame);
	initializeTimeUtility();
}

void Tag::update(float deltaTime)
{
	bool forwardVelocity = m_IOHandler.m_keyIsDown['W'];
	bool backwardVelocity = m_IOHandler.m_keyIsDown['S'];
	bool leftwardVelocity = m_IOHandler.m_keyIsDown['A'];
	bool rightwardVelocity = m_IOHandler.m_keyIsDown['D'];

	float currentSpeed = g_localUser.m_unit.m_isIt ? IT_SPEED : VICTIM_SPEED;

	g_localUser.m_unit.m_target.x += (rightwardVelocity - leftwardVelocity)*currentSpeed*deltaTime;
	g_localUser.m_unit.m_target.y += (forwardVelocity - backwardVelocity)*currentSpeed*deltaTime;
	if (forwardVelocity || backwardVelocity || leftwardVelocity || rightwardVelocity)
	{
		g_localUser.m_unit.m_hasReachedCurrentTarget = false;
	}
	g_localUser.update(deltaTime);

	//user no longer has authority to declare victory
	//if (/*Winning &&*/ !(g_localUser.m_unit.m_position == Vector2f(0,0)))
	//{
	//	CS6Packet vicPacket;
	//	vicPacket.packetType = TYPE_Victory;
	//	Color3b userColor = Color3b(g_localUser.m_unit.m_color);
	//	memcpy(vicPacket.playerColorAndID, &userColor, sizeof(userColor));
	//	memcpy(vicPacket.data.victorious.winningPlayerColorAndID, &userColor, sizeof(userColor));
	//	g_serverConnection->sendPacket(vicPacket);
	//}

	std::vector<CS6Packet> currentPendingPackets;

	CS6Packet inPacket;
	do 
	{
		inPacket = g_serverConnection->receivePackets();
		if (inPacket.packetType != 0)
		{
			currentPendingPackets.push_back(inPacket);
		}
	} while (inPacket.packetType != 0);

	std::sort(currentPendingPackets.begin(), currentPendingPackets.end(), packetComparitor);

	
	while(!currentPendingPackets.empty())
	{
		bool newUser = true;
		CS6Packet currentPacket = currentPendingPackets.back();
		currentPendingPackets.pop_back();
		if (currentPacket.packetType == TYPE_GameStart)
		{
			//Reset type things
			g_localUser.m_unit.m_position = Vector2f(currentPacket.data.reset.playerXPosition, currentPacket.data.reset.playerYPosition);
			g_localUser.m_unit.m_target = g_localUser.m_unit.m_position;
			g_localUser.m_unit.m_deadReckoningVelocity = Vector2f(0,0);
			g_localUser.m_unit.m_hasReachedCurrentTarget = false;
			Color3b packetColor = Color3b();
			memcpy(&packetColor, currentPacket.data.reset.playerColorAndID, sizeof(packetColor));
			g_localUser.m_unit.m_color = Color4f(packetColor);
			g_localUser.m_isInGame = true;
			g_localUser.m_gameID = currentPacket.gameID;
			//mark who is IT
			memcpy(&g_itColor, currentPacket.data.reset.itPlayerColorAndID, sizeof(g_itColor));

			//and tell the server that you got the reset
			CS6Packet ackBack;
			ackBack.packetType = TYPE_Acknowledge;
			g_localUser.m_lastSentPacketNum++;
			ackBack.packetNumber = g_localUser.m_lastSentPacketNum;
			memcpy(ackBack.playerColorAndID, &packetColor, sizeof(packetColor));
			ackBack.data.acknowledged.packetType = TYPE_GameStart;
			ackBack.data.acknowledged.packetNumber = currentPacket.packetNumber;
			ackBack.gameID = g_localUser.m_gameID;
			g_serverConnection->sendPacket(ackBack);

		}
		else if(currentPacket.packetType == TYPE_GameListing)
		{
			//add it to the game list
			char gameName[256];
			sprintf_s(gameName, "GameID: %d Players: %d", currentPacket.data.gameList.gameID, currentPacket.data.gameList.numPlayers);
			m_console.m_log.appendLine(gameName);
			g_localUser.m_isInGame = false;
			g_localUser.m_gameID = 0;
		}
		else if (currentPacket.packetType != 0)
		{
			Color3b packetColor = Color3b();
			memcpy(&packetColor, currentPacket.playerColorAndID, sizeof(packetColor));

			for (unsigned int ii = 0; ii < g_users.size(); ii++)
			{

				if (Color3b(g_users[ii]->m_unit.m_color) == packetColor)
				{
					newUser = false;
					g_users[ii]->processUpdatePacket(currentPacket);
				}
			}
			if (newUser)
			{
				User* tempUser = new User();
				tempUser->processUpdatePacket(currentPacket);
				tempUser->m_userType = USER_REMOTE;
				g_users.push_back(tempUser);
			}
		}
	}

	double currentTime = getCurrentTimeSeconds();

	for (unsigned int ii = 0; ii < g_users.size(); ii++)
	{
		g_users[ii]->update(deltaTime);
		//remove them if they've timed out
		if (g_users[ii]->m_lastReceivedPacketTime+5.f < currentTime)
		{
			g_users.erase(g_users.begin()+ii);
			ii--;
		}
	}

	mouseUpdate();

	m_internalTime += deltaTime;
}

void Tag::render()
{
	m_renderer.m_singleFrameBuffer.preRenderStep();

	m_worldCamera.preRenderStep();

	glUseProgram(0);

	//g_localUser.render(); //would show the "true" position if uncommented

	for (unsigned int ii = 0; ii < g_users.size(); ii++)
	{
		g_users[ii]->render();

	}

	glUseProgram(m_renderer.m_shaderID);

	m_worldCamera.postRenderStep();

	m_renderer.m_singleFrameBuffer.postRenderStep();

	for (unsigned int ii = 0; ii < g_users.size(); ii++)
	{
		glDisable(GL_DEPTH_TEST);
		glPushMatrix();
		glOrtho(1.0, 1024.0, 1.0, 1024.0/(16.0/9.0), 0, 1);
		glUseProgram(0);
		char scoreBuffer[16];
		sprintf_s(scoreBuffer, "%d", g_users[ii]->m_score);
		m_console.m_log.m_fontRenderer.drawStringWithColor(Vector3f(25.f,ii*(25.f+1),0), scoreBuffer, g_users[ii]->m_unit.m_color);
		glPopMatrix();
	}


	if(m_displayConsole)
	{
		m_console.render();
	}

	

}

bool Tag::keyDownEvent(unsigned char asKey)
{
	
	if (asKey == VK_OEM_3)
	{
		m_displayConsole = !m_displayConsole;
	}
	else if(!m_displayConsole)
	{
		m_IOHandler.KeyDownEvent(asKey);
	}
	else
	{
		if(asKey == VK_LEFT)
		{
			if(m_console.m_currentTextOffset > 0)
			{
				m_console.m_currentTextOffset--;
			}		
		}

		else if (asKey == VK_RIGHT)
		{
			if(m_console.m_currentTextOffset < m_console.m_command.size())
			{
				m_console.m_currentTextOffset++;
			}
		}
	}
	return true;
}

bool Tag::characterEvent(unsigned char asKey, unsigned char scanCode)
{
	return false;
}

bool Tag::typingEvent(unsigned char asKey)
{
	if (m_displayConsole && asKey != '`' && asKey != '~')
	{
		if(asKey == '\n' || asKey == '\r')
		{
			m_console.executeCommand();
		}
		else if (asKey == '\b')
		{
			if(m_console.m_currentTextOffset > 0)
			{
				m_console.m_command.erase(m_console.m_command.begin()+(m_console.m_currentTextOffset-1));
				m_console.m_currentTextOffset--;
			}
			
		}
		//HACK reject all other non-printable characters
		else if(asKey > 31)
		{
			m_console.insertCharacterIntoCommand(asKey);
		}
		
		return true;
	}
	else
	{
		return false;
	}
}

bool Tag::mouseEvent(unsigned int eventType)
{
	switch(eventType)
	{
	case WM_LBUTTONDOWN:
		{
			//m_playerController.pathToLocation(cursorPositionInWorld);
			break;
		}
	case WM_RBUTTONDOWN:
		{
			//m_playerController.attackTarget(cursorPositionInWorld);
			break;
		}
	}

	return true;
}
