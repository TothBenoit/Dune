#include "pch.h"
#include "EntityManager.h"
#include "Dune/Core/Logger.h"

namespace Dune
{
	EntityManager::EntityManager()
	{
		m_generationIDs.reserve(ID::GetMaximumIndex());
	}
	Entity EntityManager::CreateEntity()
	{
		EntityID id;

		if (m_freeEntityIDs.size() > 0)
		{
			id = m_freeEntityIDs.front();
			id = EntityID(ID::NextGeneration(id));
			m_generationIDs[ID::GetIndex(id)] = ID::GetGeneration(id);
			m_freeEntityIDs.pop();
		}
		else if (m_generationIDs.size() < ID::GetMaximumIndex())
		{
			id = EntityID((ID::IDType)m_generationIDs.size());
			m_generationIDs.push_back(ID::GenerationType(0));
		}
		else
		{
			LOG_CRITICAL("Maximum entities count reached");
			id = EntityID(ID::invalidID);
		}

		return Entity(id);
	}

	void EntityManager::RemoveEntity(Entity entity)
	{
		Assert(IsAlive(entity));
		EntityID id = entity.GetID();
		Assert(ID::GetIndex(id) < m_generationIDs.size());
		m_freeEntityIDs.push(id);
	}
	bool EntityManager::IsAlive(Entity entity) const
	{
		EntityID id = entity.GetID();
		ID::IDType index = ID::GetIndex(id);
		Assert(index < m_generationIDs.size());
		return m_generationIDs[index] == ID::GetGeneration(id);
	}
}
