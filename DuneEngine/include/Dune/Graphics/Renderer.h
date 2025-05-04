#pragma once

#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/CommandList.h>

namespace Dune
{
	struct Scene;
	struct Camera;

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

		struct Frame
		{
			dU32 fenceValue{ 0 };
			Graphics::CommandAllocator commandAllocator;
			Graphics::CommandList commandList;
			dQueue<Graphics::Descriptor> descriptorsToRelease;
		};

		class Renderer
		{
			friend class ImGuiWrapper;
		public:
			void Initialize(Graphics::Device& device, Graphics::Window& window);
			void Destroy();

			void OnResize(dU32 width, dU32 height);
			void Render(Scene& scene, Camera& camera);

		private:
			void WaitForFrame(const Frame& frame);

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

			Graphics::ImGuiWrapper* m_pImGui{ nullptr };
		};
	}
}