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

		struct ResourceEntry
		{
			RHI::Resource* pPhysicalResource{ nullptr };
			dVector<RHI::EResourceState> subresourceStates;
			RHI::EResourceState resourceState;
			EResourceType type;
			bool isExternal : 1;
		};

		class Renderer
		{
		public:
			static constexpr dU32 kFramesInFlight = 3;
			static constexpr dU32 kPersistentSRVCapacity = 4096;
			static constexpr dU32 kTransientSRVCapacity = 512;
			static constexpr dU32 kBarrierCapacity = 256;
			static constexpr dU32 kUploadBufferByteSize = 16 * 1024 * 1024;

			void Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, PSOCache& psoCache, Window& window);
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

			[[nodiscard]] ResourceHandle CreateTexture(const RHI::TextureDesc& desc);
			[[nodiscard]] ResourceHandle CreateBuffer(const RHI::BufferDesc& desc);
			[[nodiscard]] ResourceHandle RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] ResourceHandle RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState);
			void SetPhysicalResource(ResourceHandle handle, RHI::Resource* pResource, RHI::EResourceState state);

			[[nodiscard]] RHI::Texture& GetTexture(ResourceHandle handle);
			[[nodiscard]] RHI::Buffer& GetBuffer(ResourceHandle handle);

			[[nodiscard]] inline ResourceHandle GetDepthBufferHandle() const { return m_depthBufferHandle; }
			[[nodiscard]] inline ResourceHandle GetHDRTargetHandle() const { return m_hdrTargetHandle; }
			[[nodiscard]] inline ResourceHandle GetBackBufferHandle() const { return m_frames[m_frameIndex].backBufferHandle; }

			template<typename T>
			void RegisterRenderPass()
			{
				Assert(FindRenderPass(GetRenderTypeID<T>()) == m_passes.end());
				m_passes.push_back(MakeRenderPass<T>());
			}

			template<typename T, typename Neighbor>
			void InsertRenderPass(ERegistrationOrder order)
			{
				Assert(FindRenderPass(GetRenderTypeID<T>()) == m_passes.end());
				auto it = FindRenderPass(GetRenderTypeID<Neighbor>());
				Assert(it != m_passes.end());
				if (order == ERegistrationOrder::After)
					it = std::next(it);
				m_passes.insert(it, MakeRenderPass<T>());
			}

			template<typename Old, typename New>
			void ReplaceRenderPass()
			{
				Assert((std::is_same_v<Old, New> || FindRenderPass(GetRenderTypeID<New>()) == m_passes.end()));
				auto it = FindRenderPass(GetRenderTypeID<Old>());
				Assert(it != m_passes.end());
				DestroyRenderPass(*it);
				*it = MakeRenderPass<New>();
			}

			template<typename T>
			void UnregisterRenderPass()
			{
				auto it = FindRenderPass(GetRenderTypeID<T>());
				Assert(it != m_passes.end());
				DestroyRenderPass(*it);
				m_passes.erase(it);
			}

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
			template<typename T>
			RenderPass MakeRenderPass()
			{
				auto* pPassData = T::Create(*this);
				using PassData = decltype(pPassData);
				RenderPass pass{};
				pass.pSetup = [](RenderGraphBuilder& builder, RenderPassContext& context, void* pData) { T::Setup(builder, context, static_cast<PassData>(pData)); };
				pass.pExecute = [](RenderPassContext& context, void* pData) { T::Execute(context, static_cast<PassData>(pData)); };
				pass.pShutdown = [](Renderer& renderer, void* pData) { T::Destroy(renderer, static_cast<PassData>(pData)); };
				if constexpr (requires { T::IsPassMaterial(MaterialKey{}); })
					pass.pIsPassMaterial = &T::IsPassMaterial;
				if constexpr (requires { T::GetPipelineDesc(MaterialKey{}); })
					pass.pGetPipelineDesc = &T::GetPipelineDesc;
				if constexpr (requires { T::GetPipelineDescs(); })
					pass.pGetPipelineDescs = &T::GetPipelineDescs;
				pass.pData = pPassData;
				pass.typeID = GetRenderTypeID<T>();
				ResolvePipelines(pass);
				return pass;
			}

			dVector<RenderPass>::iterator FindRenderPass(RenderTypeID typeID);
			void DestroyRenderPass(RenderPass& pass);
			void ResolvePipelines(RenderPass& pass);

			void WaitForFrame(const Frame& frame);
			Frame& PrepareFrame();

			void TransitionResource(const ResourceAccess& access);
			void FlushBarriers(RHI::CommandList& commandList);

		private:
			RHI::Device* m_pDevice{ nullptr };
			PSOCache* m_pPSOCache{ nullptr };
			Window* m_pWindow{ nullptr };

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

			dVector<ResourceEntry> m_resources;
			dVector<RenderPass> m_passes;
			RenderPassContext m_passContext;
		};
	}
}
