#pragma once

#include <EnTT/entt.hpp>
#include "Dune/Graphics/Handles.h"

namespace Dune
{
	struct Model;
	struct Transform;

	namespace Graphics
	{
		struct ModelResources;
	}

	using EntityID = entt::entity;

	class Scene
	{
	public:
		Scene() = default;

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(Scene&&) = delete;

		void Spawn(const Model& model, const Graphics::ModelResources& resources);
		EntityID SpawnMesh(const char* pName, const Transform& transform, Graphics::MeshHandle mesh, Graphics::MaterialSetHandle materials);

	public:
		entt::registry registry;
	};
}
