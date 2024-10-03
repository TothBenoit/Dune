#pragma once

#include "EnTT/entt.hpp"
#include "Dune/Common/Pool.h"

namespace Dune
{
	namespace Graphics
	{
		struct Device;
		class Mesh;
		class Texture;
		class Pipeline;
	}

	class Renderer;
	struct SceneView;

	struct Transform
	{
		dVec3 position;
		dQuat rotation;
		dVec3 scale;
	};

	struct RenderData
	{
		Handle<Graphics::Texture> albedo;
		Handle<Graphics::Texture> normal;
		Handle<Graphics::Mesh> mesh;
	};

	struct Name
	{
		std::string name;
	};

	struct Scene
	{
		inline entt::entity CreateEntity(const char* pName)
		{
			entt::entity entity{ registry.create() };
			registry.emplace<Name>(entity, pName);
			return entity;
		}

		entt::registry registry;
	};	

	class Renderer
	{
	public:
		void Initialize();
		void Destroy();

		bool UpdateSceneView(Handle<SceneView> hView, float dt);

		Handle<SceneView> CreateSceneView();
		void DestroySceneView(Handle<SceneView> hView);

		Handle<Graphics::Texture> CreateTexture(const char* path);
		void ReleaseTexture(Handle<Graphics::Texture> hTexture);
		Handle<Graphics::Mesh> CreateMesh(const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride);
		void ReleaseMesh(Handle<Graphics::Mesh> hMesh);

		void RenderScene(Handle<SceneView> hView, const Scene& scene);

	private:
		Graphics::Device* m_pDevice{ nullptr };
		Pool<SceneView> m_sceneViewPool;
		Handle<Graphics::Pipeline> m_pbrPipeline;
	};
}