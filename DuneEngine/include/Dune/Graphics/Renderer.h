#pragma once

#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/Buffer.h>
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
		class Window;
		class Swapchain;
		class Fence;
		class CommandList;
		class CommandAllocator;
		class CommandQueue;
		class Barrier;
		class Forward;
		class DepthPrepass;

		struct Frame
		{
			dU32 fenceValue{ 0 };
			CommandAllocator commandAllocator;
			CommandList commandList;
			dQueue<Descriptor> descriptorsToRelease;
			dQueue<Buffer> buffersToRelease;
		};

		class Renderer
		{
			friend class ImGuiWrapper;
		public:
			void Initialize(Device& device, Window& window);
			void Destroy();

			void OnResize(dU32 width, dU32 height);
			void Render(Scene& scene, Camera& camera);

		private:
			void WaitForFrame(const Frame& frame);

		private:
			Device* m_pDevice{ nullptr };
			
			Window* m_pWindow{ nullptr };
			Swapchain* m_pSwapchain{ nullptr };
			Texture* m_pDepthBuffer{ nullptr };
			CommandQueue* m_pCommandQueue{ nullptr };
			
			DescriptorHeap* m_pSrvHeap{ nullptr };
			DescriptorHeap* m_pSamplerHeap{ nullptr };
			DescriptorHeap* m_pRtvHeap{ nullptr };
			DescriptorHeap* m_pDsvHeap{ nullptr };
			
			Descriptor m_renderTargetsDescriptors[3];
			Descriptor m_depthBufferDescriptor;
			
			Fence* m_pFence{ nullptr };
			Frame m_frames[3];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			Barrier* m_pBarrier{ nullptr };

			ImGuiWrapper* m_pImGui{ nullptr };

			Forward* m_pForwardPass{ nullptr };
			DepthPrepass* m_pDepthPrepass{ nullptr };
		};
	}
}