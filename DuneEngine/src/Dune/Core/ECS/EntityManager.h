#pragma once

#include "Entity.h"

namespace Dune
{
	class EntityManager
	{
	public:
		EntityManager(const EntityManager&) = delete;
		EntityManager& operator=(const EntityManager&) = delete;

	private:
		static const dU32 MAX_ENTITIES = 4096;

		dQueue<EntityID> m_freeEntityIDs;
		EntityID m_entities[MAX_ENTITIES];

	};
}