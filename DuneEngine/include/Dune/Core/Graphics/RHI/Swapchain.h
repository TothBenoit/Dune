#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Device;
	class Window;
	class CommandQueue;

	struct SwapchainDesc
	{
		dU32 latency{ 3 };
	};

	class Swapchain : public Resource 
	{
	public:
		void Initialize(Device* pDevice, Window* pWindow, CommandQueue* pCommandQueue, const SwapchainDesc& desc);
		void Destroy();

		void Resize(dU32 width, dU32 height);
		void Present();

		[[nodiscard]] void* GetBackBuffer(dU32 index);
		[[nodiscard]] dU32 GetCurrentBackBufferIndex();
		[[nodiscard]] dU32 GetLatency() const { return m_latency; };		

	private:
		dU32 m_latency{ 0 };
		void* m_pBackBuffers{ nullptr };
	};
}