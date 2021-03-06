#pragma once
#ifndef INCLUDED_CS6_PACKET_HPP
#define INCLUDED_CS6_PACKET_HPP

//Communication Protocol:
//   Client->Server: Ack
//   Server->Client: Reset
//   Client->Server: Ack

//   ------Update Loop------
//		Client->Server: Update
//		Server->ALL Clients: Update
//		ALL Clients->Server: Ack
//   ----End Update Loop----

//   Client->Server: Victory
//   Server->Client: Ack
//   Server->ALL Clients: Victory
//   ALL Clients->Server: Ack
//   Server->ALL Clients: Reset

//-----------------------------------------------------------------------------------------------
typedef unsigned char PacketType;
static const PacketType TYPE_Acknowledge = 10;
static const PacketType TYPE_Victory = 11;
static const PacketType TYPE_Update = 12;
static const PacketType TYPE_GameStart = 13;
static const PacketType TYPE_JoinGame = 14;
static const PacketType TYPE_GameListing = 15;

//-----------------------------------------------------------------------------------------------
struct AckPacket
{
	PacketType packetType;
	unsigned int packetNumber;
};

//-----------------------------------------------------------------------------------------------
struct ResetPacket
{
	float playerXPosition;
	float playerYPosition;
	//Player orientation should always start at 0 (east)
	unsigned char playerColorAndID[ 3 ];
	unsigned char itPlayerColorAndID[3];
};

//-----------------------------------------------------------------------------------------------
struct UpdatePacket
{
	float xPosition;
	float yPosition;
	float xVelocity;
	float yVelocity;
	float yawDegrees;
	//0 = east
	//+ = counterclockwise
	//range 0-359
};

//-----------------------------------------------------------------------------------------------
struct VictoryPacket
{
	unsigned char winningPlayerColorAndID[ 3 ];
	unsigned char taggedPlayerColorAndID[3];
};


//-----------------------------------------------------------------------------------------------
struct JoinGamePacket
{
	unsigned char gameID;
	unsigned char requestedColor[3]; //0,0,0 for no request
};

//-----------------------------------------------------------------------------------------------
struct GameListingPacket
{
	char gameID; //-1 if no games exist
	unsigned char numPlayers;
};

//-----------------------------------------------------------------------------------------------
struct CS6Packet
{
	PacketType packetType;
	unsigned char playerColorAndID[ 3 ];
	unsigned int packetNumber;
	double timestamp;
	unsigned char gameID;
	union PacketData
	{
		AckPacket acknowledged;
		ResetPacket reset;
		UpdatePacket updated;
		VictoryPacket victorious;
		JoinGamePacket joining;
		GameListingPacket gameList;

	} data;
};

#endif //INCLUDED_CS6_PACKET_HPP