#pragma once
#ifndef include_ENTITY
#define include_ENTITY

#include "Primitives/Element.hpp"
#include "Primitives/Vector2f.hpp"
#include "Primitives/Color4f.hpp"


class Entity : public Element
{
public:
	Entity();

	static int s_nextAssignableID;
	int m_entityID;
	Vector2f m_position; //center of the entity
	Vector2f m_target;
	Vector2f m_velocity;
	float m_orientation;//0 east, to 359 counterclockwise
	Color4f m_color;
	bool m_isIt;

	CS6Packet packForSend();
	virtual void update(float deltaSeconds);
	virtual void render();
	inline bool operator < (const Entity& rhs) const;
};


inline bool Entity::operator<(const Entity& rhs) const
{
	return m_entityID < rhs.m_entityID;
}

#endif