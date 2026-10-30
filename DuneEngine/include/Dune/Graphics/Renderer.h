#pragma once

#include <Dune/Graphics/RenderPass.h>
#include <Dune/Platform/RHI/Barrier.h>
#include <Dune/Platform/RHI/Buffer.h>
#include <Dune/Platform/RHI/CommandList.h>
#include <Dune/Platform/RHI/DescriptorHeap.h>
#include <Dune/Platform/RHI/Fence.h>
#include <Dune/Platform/RHI/Swapchain.h>
#include <Dune/Platform/RHI/Texture.h>
#include <Dune/Core/ScratchAllocator.h>
#include <Dune/Graphics/StagingMemory.h>
#include <Dune/Graphics/DescriptorAllocator.h>
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
			RHI::Buffer uploadBuffer;
			dU8* pUploadAddress{ nullptr };
			ScratchAllocator staging;
			dVector<RHI::Buffer> buffersToRelease;
			dVector<RHI::Descriptor> srvsToRelease;
			RGTexture backBufferHandle;
		};

		class Renderer
		{
		public:
			static constexpr dU32 kFramesInFlight = 3;
			static constexpr dU32 kCPUSRVCapacity = 16;
			static constexpr dU32 kBarrierCapacity = 256;
			static constexpr dU32 kUploadBufferByteSize = 16 * 1024 * 1024;

			void Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, PSOCache& psoCache, DescriptorAllocator& srvAllocator, Window& window, const RendererDesc& desc);
			void Destroy();

			void InitializeImGui(const char* pLayoutPath);
			[[nodiscard]] inline bool HasImGui() const { return m_imgui.IsInitialized(); }
			[[nodiscard]] inline RHI::ImGuiContext& GetImGui() { Assert(HasImGui()); return m_imgui; }

			void OnResize(dU32 width, dU32 height);
			void Render(const FrameData& frameData, const ViewData& view);

			[[nodiscard]] StagingMemory AllocateStaging(dU64 byteSize, dU64 alignment);

			[[nodiscard]] inline RHI::Device* GetDevice() { return m_pDevice; }
			[[nodiscard]] inline Window* GetWindow() { return m_pWindow; }
			[[nodiscard]] inline Frame& GetCurrentFrame() { return m_frames[m_frameIndex]; }
			[[nodiscard]] inline RHI::Barrier& GetBarrier() { return m_barrier; }
			[[nodiscard]] inline RHI::DescriptorHeap& GetSRVHeap() { return m_pSRVAllocator->GetHeap(); }
			[[nodiscard]] inline DescriptorAllocator& GetSRVAllocator() { return *m_pSRVAllocator; }
			[[nodiscard]] inline DescriptorAllocator& GetCPUSRVAllocator() { return m_cpuSRVAllocator; }
			inline void ReleaseSRV(RHI::Descriptor srv) { m_frames[m_frameIndex].srvsToRelease.push_back(srv); }
			[[nodiscard]] inline DescriptorAllocator& GetRTVAllocator() { return m_rtvAllocator; }
			[[nodiscard]] inline DescriptorAllocator& GetDSVAllocator() { return m_dsvAllocator; }

			[[nodiscard]] inline RenderGraph& GetRenderGraph() { return m_renderGraph; }

			[[nodiscard]] inline RGTexture GetBackBufferHandle() const { return m_frames[m_frameIndex].backBufferHandle; }
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

			[[nodiscard]] inline const PassPSOs& GetPassPSOs(dU32 typeIndex) const { return m_passPSOs[typeIndex]; }

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
			RHI::CommandQueue* m_pCommandQueue{ nullptr };

			DescriptorAllocator* m_pSRVAllocator{ nullptr };

			RHI::DescriptorHeap m_cpuSRVHeap{};
			RHI::DescriptorHeap m_rtvHeap{};
			RHI::DescriptorHeap m_dsvHeap{};
			DescriptorAllocator m_cpuSRVAllocator{};
			DescriptorAllocator m_rtvAllocator{};
			DescriptorAllocator m_dsvAllocator{};

			RHI::Fence m_fence{};
			Frame m_frames[kFramesInFlight];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			RHI::Barrier m_barrier{};
			RHI::ImGuiContext m_imgui{};

			RenderGraph m_renderGraph;
			dVector<RenderPass> m_passes;
			dVector<PassPSOs> m_passPSOs;
			RenderPassContext m_passContext;
		};
	}
}
