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
	class Scene;
	struct Camera;

	namespace Graphics
	{
		class Device;
		class Window;
		class RenderContext;

		struct Frame
		{
			dU32 fenceValue{ 0 };
			CommandAllocator commandAllocator;
			CommandList commandList;
			Descriptor backBufferRTV;
			Texture hdrTarget;
			Descriptor hdrTargetRTV;
			Descriptor hdrTargetSRV;
			ScratchDescriptorHeap srvHeap;
			ScratchDescriptorHeap samplerHeap;
			dQueue<Buffer> buffersToRelease;
		};

		class Renderer
		{
			friend class ImGuiWrapper;
		public:
			static constexpr dU32 kFramesInFlight = 3;
			static constexpr dU32 kPersistentSRVCapacity = 4096;
			static constexpr dU32 kTransientSRVCapacity = 512;

			void Initialize(RenderContext& context, Window& window);
			void Destroy();

			void OnResize(dU32 width, dU32 height);
			void Render(Scene& scene, Camera& camera);

			[[nodiscard]] inline RenderContext* GetRenderContext() { return m_pRenderContext; }
			[[nodiscard]] inline Window* GetWindow() { return m_pWindow; }
			[[nodiscard]] inline Frame& GetCurrentFrame() { return m_frames[m_frameIndex]; }
			[[nodiscard]] inline BlockDescriptorHeap& GetSRVHeap() { return m_srvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetRTVHeap() { return m_rtvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetDSVHeap() { return m_dsvHeap; }

		private:
			void WaitForFrame(const Frame& frame);

		private:
			RenderContext* m_pRenderContext{ nullptr };
			Window* m_pWindow{ nullptr };
			ImGuiWrapper* m_pImGui{ nullptr };

			Swapchain m_swapchain{};
			Texture m_depthBuffer{};
			CommandQueue m_commandQueue{};

			BlockDescriptorHeap m_srvHeap{};
			BlockDescriptorHeap m_srvImGuiHeap{};
			BlockDescriptorHeap m_rtvHeap{};
			BlockDescriptorHeap m_dsvHeap{};

			Descriptor m_depthBufferDSV;

			Buffer m_lightBuffer{};
			Buffer m_lightMatricesBuffer{};
			Descriptor m_lightsSRV{};
			Descriptor m_lightMatricesSRV{};

			dVector<Texture> m_shadowMaps{};
			dVector<Texture> m_cubeShadowMaps{};
			dVector<dMatrix4x4> m_lightMatrices{};

			Fence m_fence{};
			Frame m_frames[kFramesInFlight];
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
