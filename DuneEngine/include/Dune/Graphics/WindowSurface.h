#pragma once

#include "Dune/Platform/RHI/CommandList.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Fence.h"
#include "Dune/Platform/RHI/Swapchain.h"
#include "Dune/Platform/RHI/ImGuiContext.h"

namespace Dune
{
	class Window;
}

namespace Dune::RHI
{
	class Device;
	class Texture;
}

namespace Dune::Graphics
{
	struct WindowSurfaceDesc
	{
		bool enableImGui{ false };
		const char* pImGuiLayoutPath{ nullptr };
	};

	struct SurfaceBlit
	{
		RHI::Texture* pTexture{ nullptr };
		dU32 x{ 0 };
		dU32 y{ 0 };
	};

	class WindowSurface
	{
	public:
		static constexpr dU32 kFramesInFlight = 3;

		void Initialize(RHI::Device& device, RHI::CommandQueue& commandQueue, Window& window, const WindowSurfaceDesc& desc);
		void Destroy();

		void Present(dSpan<const SurfaceBlit> blits);

		[[nodiscard]] bool HasImGui() const { return m_imgui.IsInitialized(); }
		[[nodiscard]] RHI::ImGuiContext& GetImGui() { Assert(HasImGui()); return m_imgui; }

	private:
		struct Frame
		{
			RHI::CommandAllocator commandAllocator;
			RHI::CommandList commandList;
			dU64 fenceValue{ 0 };
		};

		void WaitForFrames();
		void CreateBackBufferViews();

	private:
		RHI::Device* m_pDevice{ nullptr };
		RHI::CommandQueue* m_pCommandQueue{ nullptr };
		Window* m_pWindow{ nullptr };
		RHI::Swapchain m_swapchain{};
		RHI::DescriptorHeap m_rtvHeap{};
		RHI::Fence m_fence{};
		Frame m_frames[kFramesInFlight];
		dU64 m_frameCount{ 0 };
		dU32 m_width{ 0 };
		dU32 m_height{ 0 };
		RHI::ImGuiContext m_imgui{};
	};
}
