#pragma once

#include <EnTT/entt.hpp>

namespace Dune 
{
	namespace Graphics
	{
		class Mesh;
		class Texture;
	}

	struct Transform
	{
		dVec3 position;
		dQuat rotation;
		float scale;
	};

	struct RenderData
	{
		dU32 albedoIdx;
		dU32 normalIdx;
		dU32 roughnessMetalnessIdx;
		dU32 meshIdx;
	};

	using EntityID = entt::entity;

	struct Scene
	{
		entt::registry registry;

		dVector<Graphics::Mesh> meshes;
		dVector<Graphics::Texture> textures;
	};
}