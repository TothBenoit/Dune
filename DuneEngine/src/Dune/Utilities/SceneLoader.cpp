#include "pch.h"
#include "Dune/Utilities/SceneLoader.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::SceneLoader
{
	bool Load(const char* path, Scene& scene, Graphics::ResourceManager& resourceManager)
	{
		FileSystem::SerializationID<EFileType::Model> id = FileSystem::Resolve<EFileType::Model>(path);
		const Graphics::ModelData& model = resourceManager.GetModel(id);
		if (model.nodes.empty())
			return false;

		for (const Graphics::ModelNode& node : model.nodes)
		{
			EntityID entity = scene.registry.create();
			scene.registry.emplace<Name>(entity).name.assign("Mesh");

			Transform& transform = scene.registry.emplace<Transform>(entity);
			transform.position = node.position;
			transform.rotation = node.rotation;
			transform.scale = 1.0f;

			RenderData& renderData = scene.registry.emplace<RenderData>(entity);
			renderData.meshIdx = node.meshIndex;
			renderData.materialSlotStart = node.materialSlotStart;
			renderData.materialSlotCount = node.materialSlotCount;
		}

		return true;
	}
}
