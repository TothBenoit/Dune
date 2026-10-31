#include "pch.h"
#include "Dune/Game/GameScene.h"
#include "Dune/Game/Component/Name.h"
#include "Dune/Game/Component/Transform.h"
#include "Dune/Game/Component/MeshInstance.h"

namespace Dune
{
	EntityID GameScene::CreateEntity([[maybe_unused]] const char* pName)
	{
		const EntityID entity = m_registry.create();
#ifdef _DEBUG
		Add(entity, Name{ pName });
#endif
		return entity;
	}

	EntityID GameScene::CreateEntity(const char* pName, const Transform& transform)
	{
		const EntityID entity = CreateEntity(pName);
		Add(entity, transform);
		return entity;
	}

	EntityID GameScene::SpawnMesh(const char* pName, const Transform& transform, MeshHandle mesh, MaterialSetHandle materials)
	{
		const EntityID entity = CreateEntity(pName, transform);
		Add(entity, MeshInstance{ .mesh = mesh, .materials = materials });
		return entity;
	}

	void GameScene::DestroyEntity(EntityID entity)
	{
		m_destroyedEntities.push_back(entity);
		m_registry.destroy(entity);
	}

	void GameScene::DestroyAllEntities()
	{
		for (const auto [entity] : m_registry.storage<EntityID>().each())
			m_destroyedEntities.push_back(entity);
		m_registry.clear();
	}

	bool GameScene::HasChanges() const
	{
		for (const dVector<EntityID>& changes : m_changes)
			if (!changes.empty())
				return true;
		return !m_destroyedEntities.empty();
	}

	void GameScene::ClearChanges()
	{
		for (dVector<EntityID>& changes : m_changes)
			changes.clear();
		m_destroyedEntities.clear();
	}
}
