#pragma once

#include <Dune/Graphics/RenderPass.h>
#include <Dune/Graphics/RenderPass/Forward.h>
#include <Dune/Graphics/RenderPass/DepthPrepass.h>
#include <Dune/Graphics/RenderPass/Tonemapping.h>
#include <Dune/Graphics/Shaders/ShaderInterop.h>
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
		class Renderer;

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

		struct LightMatrices
		{
			Buffer buffer;
			Descriptor srv;
			dVector<dMatrix4x4> matrices;
		};

		struct ShadowMaps
		{
			dVector<Texture> shadows{};
			dVector<Texture> cubeShadows{};
		};

		struct FrameLights
		{
			dVector<Light>  allActive;
			dVector<dU32> shadowCasters;
		};

		struct FrameData
		{
			FrameLights lights;
		};

		struct RenderPassContext
		{
			Renderer* pRenderer;
			Frame* pFrame;
			CommandList* pCommandList;
			Scene* pScene;
			Barrier* pBarrier;
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

			void RegisterSharedResource(EResourceTag id, void* pResource);
			[[nodiscard]] inline void* GetSharedResource(EResourceTag id) { return m_sharedResources[(dU32)id]; }

			template<typename T>
			void RegisterRenderPass()
			{
				using RenderPassData = decltype(T::Create(*this));
				m_passes.push_back({ 
					.pExecute  = [](RenderPassContext& context, void* pData) { T::Execute(context, static_cast<RenderPassData>(pData)); },
					.pShutdown = [](Renderer& renderer, void* pData) { T::Destroy(renderer, static_cast<RenderPassData>(pData)); },
					.pData = T::Create(*this),
					.desc = T::GetDesc(),
				});
			}

			[[nodiscard]] dVector<dU32>& GetShadowCastingLightsIndex() { return m_frameData.lights.shadowCasters; }
			[[nodiscard]] dVector<Light>& GetLights() { return m_frameData.lights.allActive; }

		private:
			void GatherFrameData(Scene& scene);
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

			FrameData m_frameData;

			LightMatrices m_lightMatrices;
			ShadowMaps m_shadowMaps;

			Buffer m_lightBuffer{};
			Descriptor m_lightsSRV{};

			Fence m_fence{};
			Frame m_frames[kFramesInFlight];
			dU32 m_frameIndex{ 0 };
			dU32 m_frameCount{ 0 };
			Barrier m_barrier{};

			dVector<void*> m_sharedResources;
			dVector<RenderPass> m_passes;
			Forward m_forwardPass{};
			DepthPrepass m_depthPrepass{};
			Tonemapping m_tonemappingPass{};
		};
	}
}
