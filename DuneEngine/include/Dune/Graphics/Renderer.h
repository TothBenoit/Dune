#pragma once

#include <Dune/Graphics/RenderPass.h>
#include <Dune/Graphics/RHI/Barrier.h>
#include <Dune/Graphics/RHI/Buffer.h>
#include <Dune/Graphics/RHI/CommandList.h>
#include <Dune/Graphics/RHI/DescriptorHeap.h>
#include <Dune/Graphics/RHI/Fence.h>
#include <Dune/Graphics/RHI/Swapchain.h>
#include <Dune/Graphics/RHI/Texture.h>
#include <Dune/Graphics/ScratchAllocator.h>

namespace Dune
{
	class Scene;
	class Window;
	struct Camera;

	namespace Graphics
	{
		class Device;
		class PSOCache;
		class Renderer;
		struct FrameData;

		struct Frame
		{
			dU32 fenceValue{ 0 };
			CommandAllocator commandAllocator;
			CommandList commandList;
			Descriptor backBufferRTV;
			ScratchDescriptorHeap srvHeap;
			ScratchDescriptorHeap samplerHeap;
			Buffer uploadBuffer;
			ScratchAllocator staging;
			dVector<Buffer> buffersToRelease;
			ResourceHandle backBufferHandle{ kInvalidResourceHandle };
		};

		struct ResourceEntry
		{
			Resource* pPhysicalResource{ nullptr };
			dVector<EResourceState> subresourceStates;
			EResourceState resourceState;
			EResourceType type;
			bool isExternal : 1;
		};

		class Renderer
		{
			friend class ImGuiWrapper;
		public:
			static constexpr dU32 kFramesInFlight = 3;
			static constexpr dU32 kPersistentSRVCapacity = 4096;
			static constexpr dU32 kTransientSRVCapacity = 512;
			static constexpr dU32 kBarrierCapacity = 256;
			static constexpr dU32 kUploadBufferByteSize = 16 * 1024 * 1024;

			void Initialize(Device& device, CommandQueue& commandQueue, PSOCache& psoCache, Window& window);
			void Destroy();

			void OnResize(dU32 width, dU32 height);
			void Render(const FrameData& frameData, const Camera& camera);

			[[nodiscard]] StagingMemory AllocateStaging(dU64 byteSize, dU64 alignment);

			[[nodiscard]] inline Device* GetDevice() { return m_pDevice; }
			[[nodiscard]] inline Window* GetWindow() { return m_pWindow; }
			[[nodiscard]] inline Frame& GetCurrentFrame() { return m_frames[m_frameIndex]; }
			[[nodiscard]] inline Barrier& GetBarrier() { return m_barrier; }
			[[nodiscard]] inline BlockDescriptorHeap& GetSRVHeap() { return m_srvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetRTVHeap() { return m_rtvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetDSVHeap() { return m_dsvHeap; }
			[[nodiscard]] inline const Descriptor& GetDepthBufferDSV() const { return m_depthBufferDSV; }
			[[nodiscard]] inline const Descriptor& GetHDRTargetRTV() const { return m_hdrTargetRTV; }
			[[nodiscard]] inline const Descriptor& GetHDRTargetSRV() const { return m_hdrTargetSRV; }
			[[nodiscard]] inline const Texture& GetDepthBuffer() const { return m_depthBuffer; }
			[[nodiscard]] inline const Texture& GetHDRTarget() const { return m_hdrTarget; }

			[[nodiscard]] ResourceHandle CreateTexture(const TextureDesc& desc);
			[[nodiscard]] ResourceHandle CreateBuffer(const BufferDesc& desc);
			[[nodiscard]] ResourceHandle RegisterTexture(Texture* pTexture, EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] ResourceHandle RegisterBuffer(Buffer* pBuffer, EResourceState initialState);
			void SetPhysicalResource(ResourceHandle handle, Resource* pResource, EResourceState state);

			[[nodiscard]] Texture& GetTexture(ResourceHandle handle);
			[[nodiscard]] Buffer& GetBuffer(ResourceHandle handle);

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
			void FlushBarriers(CommandList& commandList);

		private:
			Device* m_pDevice{ nullptr };
			PSOCache* m_pPSOCache{ nullptr };
			Window* m_pWindow{ nullptr };
			ImGuiWrapper* m_pImGui{ nullptr };

			Swapchain m_swapchain{};
			Texture m_depthBuffer{};
			CommandQueue* m_pCommandQueue{ nullptr };

			BlockDescriptorHeap m_srvHeap{};
			BlockDescriptorHeap m_srvImGuiHeap{};
			BlockDescriptorHeap m_rtvHeap{};
			BlockDescriptorHeap m_dsvHeap{};

			Descriptor m_depthBufferDSV;
			ResourceHandle m_depthBufferHandle{ kInvalidResourceHandle };

			Texture m_hdrTarget;
			Descriptor m_hdrTargetRTV;
			Descriptor m_hdrTargetSRV;
			ResourceHandle m_hdrTargetHandle{ kInvalidResourceHandle };

			Fence m_fence{};
			Frame m_frames[kFramesInFlight];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			Barrier m_barrier{};

			dVector<ResourceEntry> m_resources;
			dVector<RenderPass> m_passes;
			RenderPassContext m_passContext;
		};
	}
}
