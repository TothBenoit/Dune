#pragma once

namespace Dune::Graphics
{
	struct Device;
	class Window;

	struct SwapchainDesc
	{
		dU32 latency{ 3 };
	};

	class Swapchain : public Resource 
	{
	public:
		Swapchain(Device* pDevice, Window* pWindow, const SwapchainDesc& desc);
		~Swapchain();
		DISABLE_COPY_AND_MOVE(Swapchain);

		void Resize(dU32 width, dU32 height);
		void Present();

		[[nodiscard]] void* GetBackBuffer(dU32 index);
		[[nodiscard]] dU32 GetCurrentBackBufferIndex();
		[[nodiscard]] Descriptor& GetRTV(dU32 index) { Assert(index < m_latency); return m_RTVs[index]; }
		[[nodiscard]] dU32 GetLatency() const { return m_latency; };

	private:
		dU32 m_latency{ 0 };
		void* m_pBackBuffers{ nullptr };
		Descriptor* m_RTVs{ nullptr };
		Device* m_pDevice{ nullptr };
	};
}