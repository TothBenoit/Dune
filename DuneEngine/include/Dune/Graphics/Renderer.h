#pragma once

#include <Dune/Scene/Scene.h>
#include <Dune/Graphics/RenderPass.h>
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
		class RenderContext;
		class Device;
		class PSOCache;
		class Window;
		class Renderer;
		struct FrameData;

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
			Buffer uploadBuffer;
			void* pUploadAddress;
			dU32 uploadOffset;
			dVector<Buffer> buffersToRelease;
			ResourceHandle hdrTargetHandle{ kInvalidResourceHandle };
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

			void Initialize(RenderContext& context, Window& window);
			void Destroy();

			void OnResize(dU32 width, dU32 height);
			void Render(const FrameData& frameData, const Camera& camera);

			[[nodiscard]] inline Device* GetDevice() { return m_pDevice; }
			[[nodiscard]] inline PSOCache* GetPSOCache() { return m_pPSOCache; }
			[[nodiscard]] inline Window* GetWindow() { return m_pWindow; }
			[[nodiscard]] inline Frame& GetCurrentFrame() { return m_frames[m_frameIndex]; }
			[[nodiscard]] inline BlockDescriptorHeap& GetSRVHeap() { return m_srvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetRTVHeap() { return m_rtvHeap; }
			[[nodiscard]] inline BlockDescriptorHeap& GetDSVHeap() { return m_dsvHeap; }
			[[nodiscard]] inline Descriptor GetDepthBufferDSV() const { return m_depthBufferDSV; }
			[[nodiscard]] inline const Texture& GetDepthBuffer() const { return m_depthBuffer; }

			[[nodiscard]] ResourceHandle CreateTexture(const TextureDesc& desc);
			[[nodiscard]] ResourceHandle CreateBuffer(const BufferDesc& desc);
			[[nodiscard]] ResourceHandle RegisterTexture(Texture* pTexture, EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] ResourceHandle RegisterBuffer(Buffer* pBuffer, EResourceState initialState);
			void SetPhysicalResource(ResourceHandle handle, Resource* pResource, EResourceState state);

			[[nodiscard]] Texture& GetTexture(ResourceHandle handle);
			[[nodiscard]] Buffer& GetBuffer(ResourceHandle handle);

			[[nodiscard]] inline ResourceHandle GetDepthBufferHandle() const { return m_depthBufferHandle; }
			[[nodiscard]] inline ResourceHandle GetHDRTargetHandle() const { return m_frames[m_frameIndex].hdrTargetHandle; }
			[[nodiscard]] inline ResourceHandle GetBackBufferHandle() const { return m_frames[m_frameIndex].backBufferHandle; }

			template<typename T>
			void RegisterRenderPass()
			{
				RegisterRenderPass<T>(nullptr, ERegistrationOrder::After);
			}

			template<typename T, typename Neighbor>
			void RegisterRenderPass(ERegistrationOrder order)
			{
				RegisterRenderPass<T>(GetRenderPassTypeID<Neighbor>(), order);
			}

			template<typename T>
			void RegisterRenderPass(RenderPassTypeID neighborTypeID, ERegistrationOrder order)
			{
				using PassData = decltype(T::Create(*this));
				RenderPass pass{};
				pass.pSetup = [](RenderGraphBuilder& builder, RenderPassContext& context, void* pData) { T::Setup(builder, context, static_cast<PassData>(pData)); };
				pass.pExecute = [](RenderPassContext& context, void* pData) { T::Execute(context, static_cast<PassData>(pData)); };
				pass.pShutdown = [](Renderer& renderer, void* pData) { T::Destroy(renderer, static_cast<PassData>(pData)); };
				pass.pData = T::Create(*this);
				pass.typeID = GetRenderPassTypeID<T>();
				if (neighborTypeID)
				{
					auto it = std::find_if(m_passes.begin(), m_passes.end(), [=](const RenderPass& pass) { return pass.typeID == neighborTypeID; });
					Assert(it != m_passes.end());
					if (order == ERegistrationOrder::After)
						it = std::next(it);
					m_passes.insert(it, std::move(pass));
				}
				else
					m_passes.push_back(std::move(pass));
			}

			template<typename T>
			[[nodiscard]] auto Get()
			{
				using PassData = decltype(T::Create(*this));
				RenderPassTypeID typeID = GetRenderPassTypeID<T>();
				for (RenderPass& pass : m_passes)
				{
					if (pass.typeID == typeID)
						return static_cast<PassData>(pass.pData);
				}
				Assert(false);
				return PassData{ nullptr };
			}

		private:
			void WaitForFrame(const Frame& frame);

			void TransitionResource(const ResourceAccess& access);
			void FlushBarriers(CommandList& commandList);

		private:
			Device* m_pDevice{ nullptr };
			PSOCache* m_pPSOCache{ nullptr };
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
			ResourceHandle m_depthBufferHandle{ kInvalidResourceHandle };

			Fence m_fence{};
			Frame m_frames[kFramesInFlight];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			Barrier m_barrier{};

			dVector<ResourceEntry> m_resources;
			dVector<RenderPass> m_passes;
		};
	}
}
