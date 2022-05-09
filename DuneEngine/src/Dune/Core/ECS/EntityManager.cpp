#include "pch.h"
#include "EntityManager.h"
#include "Dune/Core/Logger.h"

namespace Dune
{
	EntityManager::EntityManager()
	{
		m_entities.reserve(MAX_ENTITIES);
		m_generationIDs.reserve(MAX_ENTITIES);

	}
	Entity& EntityManager::CreateEntity()
	{
		EntityID id;

		if (m_freeEntityIDs.size() > 0)
		{
			id = m_freeEntityIDs.front();
			id = EntityID(ID::NextGeneration(id));
			m_generationIDs[ID::GetIndex(id)] = EntityID(ID::GetGeneration(id) + 1);
			m_freeEntityIDs.pop();
		}
		else if (m_entities.size() < MAX_ENTITIES)
		{
			id = EntityID(m_entities.size());
			m_generationIDs.push_back(EntityID(0));
		}
		else
		{
			LOG_CRITICAL("Maximum entities count reached");
			id = EntityID(ID::invalidID);
		}

		m_entities.push_back(Entity(id));

		return m_entities.back();
	}
}
