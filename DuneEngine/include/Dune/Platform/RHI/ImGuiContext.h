#pragma once

#include "Dune/Platform/RHI/Format.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"

struct ImGuiContext;

namespace Dune
{
	class Window;
}

namespace Dune::RHI
{
	class Device;
	class CommandQueue;
	class CommandList;

	struct ImGuiDesc
	{
		Window*			pWindow{ nullptr };
		Device*			pDevice{ nullptr };
		CommandQueue*	pCommandQueue{ nullptr };
		dU32			framesInFlight{ 0 };
		EFormat			renderTargetFormat{ EFormat::R8G8B8A8_UNORM };
		const char*		pLayoutPath{ nullptr };
	};

	class ImGuiContext
	{
		inline static constexpr dU32 kDefaultHookPriority = 1000;
		inline static constexpr dU32 kSRVCapacity = 64;
	public:
		class [[nodiscard]] LockGuard
		{
		public:
			LockGuard(LockGuard&&) = delete;
			LockGuard& operator=(LockGuard&&) = delete;
			LockGuard(const LockGuard&) = delete;
			LockGuard& operator=(const LockGuard&) = delete;
			~LockGuard() { m_pContext->Unlock(); }
		private:
			friend class ImGuiContext;
			explicit LockGuard(ImGuiContext& context) : m_pContext(&context) {}
			ImGuiContext* m_pContext;
		};

		void Initialize(const ImGuiDesc& desc, dU32 priority = kDefaultHookPriority);
		[[nodiscard]] LockGuard AcquireContext();
		void NewFrame(const LockGuard& guard);
		void Render(CommandList& commandList);
		void Destroy();

		[[nodiscard]] bool IsInitialized() const { return m_pContext != nullptr; }

	private:
		void Lock();
		void Unlock();

	private:
		::ImGuiContext* m_pContext{ nullptr };
		Window* m_pWindow{ nullptr };
		BlockDescriptorHeap m_srvHeap{};
		dU32 m_hookHandle{ 0 };
		dString m_layoutPath;
	};
}
