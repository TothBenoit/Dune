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

	struct Scene
	{
		entt::registry registry;

		dVector<Graphics::Mesh> meshes;
		dVector<Graphics::Texture> textures;
		dVector<Graphics::MaterialData> materials;
	};
}