#include "pch.h"
#include "Dune/World/Scene.h"
#include "Dune/World/Component/Name.h"
#include "Dune/World/Component/Transform.h"
#include "Dune/World/Component/MeshInstance.h"
#include "Dune/Assets/Model.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune
{
	void Scene::Spawn(const Model& model, const Graphics::ModelResources& resources)
	{
		for (dU32 nodeIndex = 0; nodeIndex < (dU32)model.nodes.size(); nodeIndex++)
		{
			const ModelNode& node = model.nodes[nodeIndex];
			const Transform transform
			{
				.position = node.position,
				.rotation = DirectX::XMLoadFloat4(&node.rotation),
				.scale = 1.0f,
			};
			SpawnMesh("Mesh", transform, resources.meshes[node.mesh], resources.nodeMaterialSets[nodeIndex]);
		}
	}

	EntityID Scene::SpawnMesh(const char* pName, const Transform& transform, Graphics::MeshHandle mesh, Graphics::MaterialSetHandle materials)
	{
		const EntityID entity = registry.create();
		registry.emplace<Name>(entity, pName);
		registry.emplace<Transform>(entity, transform);
		registry.emplace<MeshInstance>(entity, MeshInstance{ .mesh = mesh, .materials = materials });
		return entity;
	}
}
