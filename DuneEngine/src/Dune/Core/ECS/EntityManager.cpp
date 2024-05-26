#include "pch.h"
#include "Dune/Core/ECS/EntityManager.h"
#include "Dune/Core/Logger.h"
#include "Dune/Utilities/StringUtils.h"

namespace Dune
{
	EntityID EntityManager::CreateEntity()
	{
		EntityID id;

		if (m_freeEntityIDs.size() > 0)
		{
			id = m_freeEntityIDs.front();
			m_freeEntityIDs.pop();
		}
		else
		{
			Assert(m_generationIDs.size() < ID::GetMaximumIndex());
			id = EntityID((ID::IDType)m_generationIDs.size());
			m_generationIDs.push_back(ID::GenerationType(0));
		}

		return EntityID(id);
	}

	//To remove an Entity we have to invalidate its ID and remove its components
	void EntityManager::RemoveEntity(EntityID id)
	{
		Assert(IsValid(id));


		//Generate new ID
		EntityID newID = EntityID(ID::NextGeneration(id));
		m_freeEntityIDs.push(newID);
		
		//Invalidate previous ID
		m_generationIDs[ID::GetIndex(id)] = ID::GetGeneration(newID);
		
		LOG_INFO(dStringUtils::printf("Entity %u has been removed", id).c_str());
	}
	bool EntityManager::IsValid(EntityID id) const
	{
		const ID::IDType index = ID::GetIndex(id);
		Assert(index < m_generationIDs.size());
		const ID::GenerationType generation = ID::GetGeneration(id);
		return m_generationIDs[index] == generation;
	}
}
