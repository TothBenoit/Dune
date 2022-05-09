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
		Entity CreateEntity();
		void RemoveEntity(Entity entity);
		bool IsAlive(Entity entity) const ;

	private:
		static const dU32 MAX_ENTITIES = ID::GetMaximumIndex();

		dQueue<EntityID> m_freeEntityIDs;
		dVector<ID::GenerationType> m_generationIDs;
		Entity m_entities[MAX_ENTITIES];
	};
}