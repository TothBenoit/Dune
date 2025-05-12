#pragma once

#include <EnTT/entt.hpp>

namespace Dune 
{
	namespace Graphics
	{
		class Mesh;
		class Texture;
		struct MaterialData;
	}

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

	using EntityID = entt::entity;

	struct Scene
	{
		entt::registry registry;

		dVector<Graphics::Mesh> meshes;
		dVector<Graphics::Texture> textures;
		dVector<Graphics::MaterialData> materials;
	};
}