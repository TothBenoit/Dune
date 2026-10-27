#pragma once

#include <Dune/Graphics/RenderPass.h>
#include <Dune/Platform/RHI/Barrier.h>
#include <Dune/Platform/RHI/Buffer.h>
#include <Dune/Platform/RHI/CommandList.h>
#include <Dune/Platform/RHI/DescriptorHeap.h>
#include <Dune/Platform/RHI/Fence.h>
#include <Dune/Platform/RHI/Swapchain.h>
#include <Dune/Platform/RHI/Texture.h>
#include <Dune/Graphics/ScratchAllocator.h>
#include <Dune/Platform/RHI/ImGuiContext.h>

namespace Dune::RHI
{
	class Device;
}

namespace Dune
{
	class Scene;
	class Window;

	namespace Graphics
	{
		class PSOCache;
		class Renderer;
		struct FrameData;
		struct ViewData;
		struct RendererDesc;

		struct Frame
		{
			dU32 fenceValue{ 0 };
			RHI::CommandAllocator commandAllocator;
			RHI::CommandList commandList;
			RHI::Descriptor backBufferRTV;
			RHI::ScratchDescriptorHeap srvHeap;
			RHI::ScratchDescriptorHeap samplerHeap;
			RHI::Buffer uploadBuffer;
			ScratchAllocator staging;
			dVector<RHI::Buffer> buffersToRelease;
			ResourceHandle backBufferHandle{ kInvalidResourceHandle };
		};

		class Renderer
		{
		public:
			static constexpr dU32 kFramesInFlight = 3;
			static constexpr dU32 kPersistentSRVCapacity = 4096;
			static constexpr dU32 kTransientSRVCapacity = 512;
			static constexpr dU32 kBarrierCapacity = 256;
			static constexpr dU32 kUploadBufferByteSize = 16 * 1024 * 1024;

			void Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, PSOCache& psoCache, Window& window, const RendererDesc& desc);
			void Destroy();

			void InitializeImGui(const char* pLayoutPath);
			[[nodiscard]] inline bool HasImGui() const { return m_imgui.IsInitialized(); }
			[[nodiscard]] inline RHI::ImGuiContext& GetImGui() { Assert(HasImGui()); return m_imgui; }

			void OnResize(dU32 width, dU32 height);
			void Render(const FrameData& frameData, const ViewData& view);

			[[nodiscard]] RHI::StagingMemory AllocateStaging(dU64 byteSize, dU64 alignment);

			[[nodiscard]] inline RHI::Device* GetDevice() { return m_pDevice; }
			[[nodiscard]] inline Window* GetWindow() { return m_pWindow; }
			[[nodiscard]] inline Frame& GetCurrentFrame() { return m_frames[m_frameIndex]; }
			[[nodiscard]] inline RHI::Barrier& GetBarrier() { return m_barrier; }
			[[nodiscard]] inline RHI::BlockDescriptorHeap& GetSRVHeap() { return m_srvHeap; }
			[[nodiscard]] inline RHI::BlockDescriptorHeap& GetRTVHeap() { return m_rtvHeap; }
			[[nodiscard]] inline RHI::BlockDescriptorHeap& GetDSVHeap() { return m_dsvHeap; }
			[[nodiscard]] inline const RHI::Descriptor& GetDepthBufferDSV() const { return m_depthBufferDSV; }
			[[nodiscard]] inline const RHI::Descriptor& GetHDRTargetRTV() const { return m_hdrTargetRTV; }
			[[nodiscard]] inline const RHI::Descriptor& GetHDRTargetSRV() const { return m_hdrTargetSRV; }
			[[nodiscard]] inline const RHI::Texture& GetDepthBuffer() const { return m_depthBuffer; }
			[[nodiscard]] inline const RHI::Texture& GetHDRTarget() const { return m_hdrTarget; }

			[[nodiscard]] inline RenderGraph& GetRenderGraph() { return m_renderGraph; }

			[[nodiscard]] inline ResourceHandle GetDepthBufferHandle() const { return m_depthBufferHandle; }
			[[nodiscard]] inline ResourceHandle GetHDRTargetHandle() const { return m_hdrTargetHandle; }
			[[nodiscard]] inline ResourceHandle GetBackBufferHandle() const { return m_frames[m_frameIndex].backBufferHandle; }
			[[nodiscard]] inline dUVec2 GetResolution() const { return m_resolution; }

			void ResolvePSOs(MaterialKey key);

			template<typename T>
			void ResolvePSOs(MaterialKey key)
			{
				auto it = FindRenderPass(GetRenderTypeID<T>());
				Assert(it != m_passes.end());
				ResolvePSO(*it, key);
			}

			PSO* ResolvePSO(RenderPass& pass, MaterialKey key);

		private:
			dVector<RenderPass>::iterator FindRenderPass(RenderTypeID typeID);
			void ResolvePipelines(RenderPass& pass);

			void WaitForFrame(const Frame& frame);
			Frame& PrepareFrame();

			void FlushBarriers(RHI::CommandList& commandList);

		private:
			RHI::Device* m_pDevice{ nullptr };
			PSOCache* m_pPSOCache{ nullptr };
			Window* m_pWindow{ nullptr };
			dUVec2 m_resolution{ 0, 0 };

			RHI::Swapchain m_swapchain{};
			RHI::Texture m_depthBuffer{};
			RHI::CommandQueue* m_pCommandQueue{ nullptr };

			RHI::BlockDescriptorHeap m_srvHeap{};
			RHI::BlockDescriptorHeap m_rtvHeap{};
			RHI::BlockDescriptorHeap m_dsvHeap{};

			RHI::Descriptor m_depthBufferDSV;
			ResourceHandle m_depthBufferHandle{ kInvalidResourceHandle };

			RHI::Texture m_hdrTarget;
			RHI::Descriptor m_hdrTargetRTV;
			RHI::Descriptor m_hdrTargetSRV;
			ResourceHandle m_hdrTargetHandle{ kInvalidResourceHandle };

			RHI::Fence m_fence{};
			Frame m_frames[kFramesInFlight];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			RHI::Barrier m_barrier{};
			RHI::ImGuiContext m_imgui{};

			RenderGraph m_renderGraph;
			dVector<RenderPass> m_passes;
			RenderPassContext m_passContext;
		};
	}
}
