#pragma once

#include <EnTT/entt.hpp>
#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct Transform;

	using EntityID = entt::entity;

	class GameScene
	{
	public:
		GameScene() = default;

		GameScene(const GameScene&) = delete;
		GameScene& operator=(const GameScene&) = delete;
		GameScene(GameScene&&) = delete;
		GameScene& operator=(GameScene&&) = delete;

		EntityID CreateEntity(const char* pName, const Transform& transform);
		EntityID SpawnMesh(const char* pName, const Transform& transform, MeshHandle mesh, MaterialSetHandle materials);

		template<typename T>
		T& AddComponent(EntityID entity, T component)
		{
			return registry.emplace<T>(entity, std::move(component));
		}

		template<typename... T>
		[[nodiscard]] auto View() const
		{
			return registry.view<const T...>();
		}

	public:
		entt::registry registry;
	};
}
