#pragma once

#include <EnTT/entt.hpp>

namespace Dune
{
	struct Transform
	{
		dVec3 position;
		dQuat rotation;
		float scale;
	};

	struct RenderData
	{
		dU32 materialIdx;
		dU32 meshIdx;
	};

	struct Name
	{
		dString name;
	};

	enum ELightType
	{
		Directional,
		Point,
		Spot
	};

	struct Light
	{
		dVec3       color;
		float       intensity;
		dVec3       position;
		float       range;
		dVec3       direction;
		float       angle;
		float       penumbra;
		ELightType  type;
		bool        castShadow;
	};

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
