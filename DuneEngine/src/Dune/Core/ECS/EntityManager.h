#pragma once

#include "Entity.h"

namespace Dune
{
	class EntityManager
	{
	public:
		EntityManager();
		EntityManager(const EntityManager&) = delete;
		EntityManager& operator=(const EntityManager&) = delete;
		Entity& CreateEntity();


	private:
		static const dU32 MAX_ENTITIES = ID::GetMaximumIndex();

		dQueue<EntityID> m_freeEntityIDs;
		dVector<Entity> m_entities;
		dVector<EntityID> m_generationIDs;

	};
}