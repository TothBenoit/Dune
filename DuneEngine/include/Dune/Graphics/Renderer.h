#pragma once

#include <Dune/Graphics/RenderPass/Shadow.h>
#include <Dune/Graphics/RenderPass/Forward.h>
#include <Dune/Graphics/RenderPass/DepthPrepass.h>
#include <Dune/Graphics/RenderPass/Tonemapping.h>
#include <Dune/Graphics/RHI/Barrier.h>
#include <Dune/Graphics/RHI/Buffer.h>
#include <Dune/Graphics/RHI/CommandList.h>
#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/Fence.h>
#include <Dune/Graphics/RHI/Swapchain.h>
#include <Dune/Graphics/RHI/Texture.h>

namespace Dune
{
	struct Scene;
	struct Camera;

	namespace Graphics
	{
		class Device;
		class Window;

		struct Frame
		{
			dU32 fenceValue{ 0 };
			CommandAllocator commandAllocator;
			CommandList commandList;
			Descriptor backBufferRTV;
			Texture colorTarget;
			Descriptor colorTargetRTV;
			Descriptor colorTargetSRV;
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
			ImGuiWrapper* m_pImGui{ nullptr };

			Swapchain m_swapchain{};
			Texture m_depthBuffer{};
			CommandQueue m_commandQueue{};
			
			DescriptorHeap m_srvHeap{};
			DescriptorHeap m_samplerHeap{};
			DescriptorHeap m_rtvHeap{};
			DescriptorHeap m_dsvHeap{};
			
			Descriptor m_depthBufferDSV;
			
			Buffer m_lightBuffer{};
			Buffer m_lightMatricesBuffer{};
			Descriptor m_lightsSRV{};
			Descriptor m_lightMatricesSRV{};

			dVector<Texture> m_shadowMaps{};
			dVector<Texture> m_cubeShadowMaps{};
			dVector<dMatrix4x4> m_lightMatrices{};

			Fence m_fence{};
			Frame m_frames[3];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			Barrier m_barrier{};

			Shadow m_shadowPass{};
			Forward m_forwardPass{};
			DepthPrepass m_depthPrepass{};
			Tonemapping m_tonemappingPass{};
		};
	}
}