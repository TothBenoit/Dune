#include "pch.h"
#include "Dune/Game/GameScene.h"
#include "Dune/Game/Component/Name.h"
#include "Dune/Game/Component/Transform.h"
#include "Dune/Game/Component/MeshInstance.h"

namespace Dune
{
	EntityID GameScene::CreateEntity(const char* pName, const Transform& transform)
	{
		const EntityID entity = registry.create();
		registry.emplace<Name>(entity, pName);
		registry.emplace<Transform>(entity, transform);
		return entity;
	}

	EntityID GameScene::SpawnMesh(const char* pName, const Transform& transform, MeshHandle mesh, MaterialSetHandle materials)
	{
		const EntityID entity = CreateEntity(pName, transform);
		AddComponent(entity, MeshInstance{ .mesh = mesh, .materials = materials });
		return entity;
	}
}
