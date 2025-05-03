#pragma once

#include "EnTT/entt.hpp"

namespace Dune::Core 
{
	struct Transform
	{
		dVec3 position;
		dQuat rotation;
		float scale;
	};

	using EntityID = entt::entity;

	struct Scene
	{
		entt::registry registry;
	};
}