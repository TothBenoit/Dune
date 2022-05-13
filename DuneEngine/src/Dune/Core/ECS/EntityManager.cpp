#include "pch.h"
#include "EntityManager.h"
#include "Dune/Core/EngineCore.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/Logger.h"

namespace Dune
{
	EntityManager::EntityManager()
	{
		m_generationIDs.reserve(ID::GetMaximumIndex());
	}
	EntityID EntityManager::CreateEntity()
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
			return EntityID(ID::invalidID);
		}

		LOG_INFO(dStringUtils::printf("Entity %u has been removed", id).c_str());
		return EntityID(id);
	}

	void EntityManager::RemoveEntity(EntityID entity)
	{
		Assert(IsAlive(entity));
		Assert(ID::GetIndex(entity) < m_generationIDs.size());
		m_freeEntityIDs.push(entity);
		
		LOG_INFO(dStringUtils::printf("Entity %u has been removed", entity).c_str());

		//Dirty, really need to factorize this or do it in an another way

		ComponentManager<TransformComponent>* pTransformManager = EngineCore::GetTransformManager();
		if (pTransformManager->Contains(entity))
		{
			pTransformManager->Remove(entity);
		}

		ComponentManager<BindingComponent>* pBindingManager = EngineCore::GetBindingManager();
		if (pBindingManager->Contains(entity))
		{
			pBindingManager->Remove(entity);
		}

		ComponentManager<GraphicsComponent>* pGraphicsManager = EngineCore::GetGraphicsManager();
		if (pGraphicsManager->Contains(entity))
		{
			pGraphicsManager->Remove(entity);
		}
	}
	bool EntityManager::IsAlive(EntityID entity) const
	{
		EntityID id = entity;
		ID::IDType index = ID::GetIndex(id);
		Assert(index < m_generationIDs.size());
		return m_generationIDs[index] == ID::GetGeneration(id);
	}
}
