#pragma once

struct ImGuiContext;

namespace Dune::Graphics
{
	class Window;
	class Renderer;
	class CommandList;

	class ImGuiWrapper
	{
		inline static constexpr dU32 kDefaultHookPriority = 1000;
	public:
		class [[nodiscard]] LockGuard
		{
		public:
			LockGuard(LockGuard&&) = delete;
			LockGuard& operator=(LockGuard&&) = delete;
			LockGuard(const LockGuard&) = delete;
			LockGuard& operator=(const LockGuard&) = delete;
			~LockGuard() { m_pWrapper->Unlock(); }
		private:
			friend class ImGuiWrapper;
			explicit LockGuard(ImGuiWrapper& wrapper) : m_pWrapper(&wrapper) {}
			ImGuiWrapper* m_pWrapper;
		};

		void Initialize(Window& window, Renderer& renderer, dU32 priority = kDefaultHookPriority);
		[[nodiscard]] LockGuard AcquireContext();
		void NewFrame(const LockGuard& guard);
		void Render(CommandList& commandList);
		void Destroy();

	private:
		void Lock();
		void Unlock();

	private:
		ImGuiContext* m_pContext{ nullptr };
		Window* m_pWindow{ nullptr };
		Renderer* m_pRenderer{ nullptr };
		dU32 m_hookHandle{ 0 };
	};
}
