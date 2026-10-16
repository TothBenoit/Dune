#include "pch.h"
#include "Dune/Utilities/SceneLoader.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Component/Transform.h"
#include "Dune/Component/Name.h"
#include "Dune/Component/MeshInstance.h"
#include "Dune/Graphics/ResourceManager.h"

namespace Dune::SceneLoader
{
	void Spawn(Scene& scene, const Graphics::ModelData& model, const Graphics::ModelResources& resources)
	{
		for (dU32 nodeIndex = 0; nodeIndex < (dU32)model.nodes.size(); nodeIndex++)
		{
			const Graphics::ModelNode& node = model.nodes[nodeIndex];
			EntityID entity = scene.registry.create();
			scene.registry.emplace<Name>(entity).name.assign("Mesh");

			Transform& transform = scene.registry.emplace<Transform>(entity);
			transform.position = node.position;
			transform.rotation = DirectX::XMLoadFloat4(&node.rotation);
			transform.scale = 1.0f;

			MeshInstance& meshInstance = scene.registry.emplace<MeshInstance>(entity);
			meshInstance.mesh = resources.meshes[node.mesh];
			meshInstance.materials = resources.nodeMaterialSets[nodeIndex];
		}
	}
}
