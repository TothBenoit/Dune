#pragma once

#include <EnTT/entt.hpp>

namespace Dune
{
	using EntityID = entt::entity;

	class Scene
	{
	public:
		Scene() = default;

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(Scene&&) = delete;

	public:
		entt::registry registry;
	};
}
