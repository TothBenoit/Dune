#pragma once

struct ImGuiContext;

namespace Dune::Graphics
{
	class Window;
	class Renderer;
	class CommandList;

	class ImGuiWrapper
	{
	public:
		void Initialize(Window& window, Renderer& renderer);
		void NewFrame();
		void Render(CommandList& commandList);
		void Destroy();
		
		void Lock();
		void Unlock();

	private:
		ImGuiContext* m_pContext{ nullptr };
		Window* m_pWindow{ nullptr };
		Renderer* m_pRenderer{ nullptr };
	};
}