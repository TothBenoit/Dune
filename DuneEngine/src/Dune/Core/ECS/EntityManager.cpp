#include "pch.h"
#include "EntityManager.h"
#include "Dune/Core/EngineCore.h"
#include "Dune/Utilities/StringUtils.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/ECS/Components/TransformComponent.h"
#include "Dune/Core/ECS/Components/BindingComponent.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"
#include "Dune/Core/ECS/Components/CameraComponent.h"
#include "Dune/Core/ECS/Components/PointLightComponent.h"

namespace Dune
{
	EntityManager::EntityManager()
	{
		m_generationIDs.reserve(MAX_ENTITIES);
	}

	EntityID EntityManager::CreateEntity()
	{
		EntityID id;

		if (m_freeEntityIDs.size() > 0)
		{
			id = m_freeEntityIDs.front();
			m_freeEntityIDs.pop();
			id = EntityID(id);
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

		LOG_INFO(dStringUtils::printf("Entity %u has been created", id).c_str());
		return EntityID(id);
	}

	//To remove an Entity we have to invalidate its ID and remove its components
	void EntityManager::RemoveEntity(EntityID id)
	{
		Assert(IsValid(id));
		Assert(ID::GetIndex(id) < m_generationIDs.size());

		//Generate new ID
		EntityID newID = EntityID(ID::NextGeneration(id));
		m_freeEntityIDs.push(newID);
		
		//Invalidate previous ID
		m_generationIDs[ID::GetIndex(id)] = ID::GetGeneration(newID);
		
		//Remove its components
		//TODO : Create a component list ?
		EngineCore::RemoveComponent<TransformComponent>(id);
		EngineCore::RemoveComponent<BindingComponent>(id);
		EngineCore::RemoveComponent<GraphicsComponent>(id);
		EngineCore::RemoveComponent<CameraComponent>(id);
		EngineCore::RemoveComponent<PointLightComponent>(id);
		LOG_INFO(dStringUtils::printf("Entity %u has been removed", id).c_str());
	}
	bool EntityManager::IsValid(EntityID id) const
	{
		ID::IDType index = ID::GetIndex(id);
		Assert(index < m_generationIDs.size());
		return m_generationIDs[index] == ID::GetGeneration(id);
	}
}
