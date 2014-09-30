#include "CS6Packet.hpp"
#include "Primitives/Color3b.hpp"
#include "Entity.hpp"



int Entity::s_nextAssignableID = 0;

//----------------------------------------------------------------------------------------
Entity::Entity()
{
	m_entityID = s_nextAssignableID;
	s_nextAssignableID++;
	m_target = Vector2f(0,0);
	m_velocity = Vector2f(0,0);
	m_orientation = 0;
	m_isIt = false;
}

//----------------------------------------------------------------------------------------
void Entity::update(float deltaSeconds)
{
	Vector2f velocity = m_target - m_position;
	if (!(velocity == Vector2f(0,0)))
	{
		velocity = velocity.normalized();
		m_position = m_position + (velocity * deltaSeconds * 50.f);
	}
	
}

//----------------------------------------------------------------------------------------
void Entity::render()
{
	//NO RENDERING SERVER SIDE
	//glPushMatrix();
	//glTranslatef(m_position.x, m_position.y, 0.f);
	////glScalef(m_blueprint->m_collisionBoundsSize.x, m_blueprint->m_collisionBoundsSize.y, 0.f);
	//
	//glColor4f(m_color.red, m_color.green, m_color.blue, 1.f);
	//glBegin(GL_QUADS);
	//{
	//	glVertex2f(-0.5f, -0.5f);
	//	glVertex2f(0.5f, -0.5f);
	//	glVertex2f(0.5f, 0.5f);
	//	glVertex2f(-0.5f, 0.5f);
	//}
	//glEnd();
	//
	//glPopMatrix();
}

//----------------------------------------------------------------------------------------
//Pack up the position data in to a packet
CS6Packet Entity::packForSend()
{
	CS6Packet preparedPacket;
	preparedPacket.packetType = TYPE_Update;
	Color3b cleanedColor = Color3b(m_color);
	memcpy(preparedPacket.playerColorAndID, &cleanedColor, sizeof(cleanedColor));
	preparedPacket.data.updated.xPosition = m_position.x;
	preparedPacket.data.updated.yPosition = m_position.y;
	preparedPacket.data.updated.xVelocity = m_velocity.x;
	preparedPacket.data.updated.yVelocity = m_velocity.y;
	preparedPacket.data.updated.yawDegrees = m_orientation;
	preparedPacket.packetNumber = 0;
	return preparedPacket;
}