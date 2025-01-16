#pragma once

#include "EnTT/entt.hpp"
#include "Dune/Common/Pool.h"
#include <Dune/Utilities/SimpleCameraController.h>
#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/CommandList.h>

namespace Dune
{
	namespace Graphics
	{
		class Device;
		class Mesh;
		class Texture;
		class GraphicsPipeline;
		class Buffer;
		class Window;
		class Swapchain;
		class Fence;
		class CommandList;
		class CommandAllocator;
		class CommandQueue;
		class Barrier;
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
		Graphics::Texture* pAlbedo;
		Graphics::Texture* pNormal;
		const Graphics::Mesh* pMesh;
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

	struct Frame
	{
		dU32 fenceValue{ 0 };
		Graphics::CommandAllocator commandAllocator;
		Graphics::CommandList commandList;
		dQueue<Graphics::Descriptor> descriptorsToRelease;
	};

	class Renderer
	{
	public:
		void Initialize(Graphics::Device* pDevice);
		void Destroy();

		void OnResize(dU32 width, dU32 height);

		bool UpdateSceneView(float dt);

		void RenderScene(const Scene& scene);

	private:

		void WaitForFrame(const Frame& frame);

	public:
		SimpleCameraController m_cameraController; // TODO : Don't use the controller here, put only the camera.

	private:
		Graphics::Device* m_pDevice{ nullptr };

		Graphics::Window* m_pWindow{ nullptr };
		Graphics::Swapchain* m_pSwapchain{ nullptr };
		Graphics::GraphicsPipeline* m_pPbrPipeline{ nullptr };
		Graphics::Texture* m_pDepthBuffer{ nullptr };
		Graphics::CommandQueue* m_pCommandQueue{ nullptr };

		Graphics::DescriptorHeap* m_pSrvHeap{ nullptr };
		Graphics::DescriptorHeap* m_pSamplerHeap{ nullptr };
		Graphics::DescriptorHeap* m_pRtvHeap{ nullptr };
		Graphics::DescriptorHeap* m_pDsvHeap{ nullptr };

		Graphics::Descriptor m_renderTargetsDescriptors[3];
		Graphics::Descriptor m_depthBufferDescriptor;

		Graphics::Fence* m_pFence{ nullptr };
		Frame m_frames[3];
		dU32 m_frameIndex{ 0 };
		dU32 m_frameCount{ 0 };
		Graphics::Barrier* m_pBarrier{ nullptr };

		void* m_pOnResizeData{ nullptr };
	};
}