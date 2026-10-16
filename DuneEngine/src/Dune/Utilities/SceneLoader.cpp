#include "pch.h"
#include "Dune/Utilities/SceneLoader.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Component/Transform.h"
#include "Dune/Component/Name.h"
#include "Dune/Component/MeshInstance.h"
#include "Dune/Asset/AssetCache.h"

namespace Dune::SceneLoader
{
	void Spawn(Scene& scene, const Model& model)
	{
		for (const ModelNode& node : model.nodes)
		{
			EntityID entity = scene.registry.create();
			scene.registry.emplace<Name>(entity).name.assign("Mesh");

			Transform& transform = scene.registry.emplace<Transform>(entity);
			transform.position = node.position;
			transform.rotation = DirectX::XMLoadFloat4(&node.rotation);
			transform.scale = 1.0f;

			MeshInstance& meshInstance = scene.registry.emplace<MeshInstance>(entity);
			meshInstance.mesh = node.mesh;
			meshInstance.materials = node.materials;
		}
	}
}
