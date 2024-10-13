#pragma once

#include "EnTT/entt.hpp"
#include "Dune/Common/Pool.h"
#include <Dune/Utilities/SimpleCameraController.h>

namespace Dune
{
	namespace Graphics
	{
		struct Device;
		class Mesh;
		class Texture;
		class GraphicsPipeline;
		class Buffer;
		struct DirectCommand;
		class View;
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
		void Initialize(Graphics::Device* pDevice);
		void Destroy();

		void OnResize(dU32 width, dU32 height);

		bool UpdateSceneView(float dt);

		void RenderScene(const Scene& scene);

	public:
		SimpleCameraController m_cameraController; // TODO : Don't use the controller here, put only the camera.

	private:
		Graphics::Device* m_pDevice{ nullptr };
		Graphics::View* m_pView{ nullptr };
		Graphics::DirectCommand* m_pCommand{ nullptr };

		Handle<Graphics::GraphicsPipeline> m_pbrPipeline;
		Handle<Graphics::Texture> m_depthBuffer;
		Handle<Graphics::Buffer> m_globalsBuffer;

		void* m_pOnResizeData{ nullptr };
	};
}