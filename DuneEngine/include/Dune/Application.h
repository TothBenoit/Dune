#pragma once

#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/RHI/ImGuiWrapper.h"

namespace Dune
{
	class Scene;
	struct Camera;

	struct FileSystemRoot
	{
		dString name;
		dString path;
	};

	struct ApplicationDesc
	{
		dU32 workerCount{ 3 };
		dString engineResourcePath;
		dVector<FileSystemRoot> roots;
	};

	struct ViewDesc
	{
		dWString title{ L"Dune" };
		dU32 width{ 1600 };
		dU32 height{ 900 };
		bool enableImGui{ false };
	};

	using ViewHandle = dU32;
	inline constexpr ViewHandle kInvalidViewHandle{ dU32(-1) };

	class Application
	{
	public:
		void Initialize(const ApplicationDesc& desc);
		void Destroy();

		[[nodiscard]] ViewHandle CreateView(const ViewDesc& desc);
		void DestroyView(ViewHandle view);

		[[nodiscard]] bool BeginFrame();
		void Render(ViewHandle view, const Scene& scene, const Camera& camera);
		void EndFrame();

		void LoadModel(Scene& scene, const char* pPath);

		[[nodiscard]] bool IsViewOpen(ViewHandle view) const;
		[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] Graphics::Window& GetWindow(ViewHandle view);
		[[nodiscard]] Graphics::Renderer& GetRenderer(ViewHandle view);
		[[nodiscard]] Graphics::ImGuiWrapper& GetImGui(ViewHandle view);
		[[nodiscard]] Graphics::RenderContext& GetRenderContext() { return m_renderContext; }

	private:
		struct View
		{
			Graphics::Window window;
			Graphics::Renderer renderer;
			Graphics::ImGuiWrapper imgui;
			dU32 resizeEventHandle{ 0 };
			bool hasImGui{ false };
		};

		struct SceneFrame
		{
			const Scene* pScene{ nullptr };
			Graphics::FrameData frameData;
		};

		[[nodiscard]] View& GetView(ViewHandle view);
		[[nodiscard]] const Graphics::FrameData& GetFrameData(const Scene& scene);

	private:
		Graphics::RenderContext m_renderContext{};
		dVector<std::unique_ptr<View>> m_views;
		dVector<SceneFrame> m_sceneFrames;
		dU32 m_sceneFrameCount{ 0 };
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		float m_deltaTime{ 0.0f };
	};
}
