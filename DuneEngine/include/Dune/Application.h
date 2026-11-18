#pragma once

#include "Dune/Graphics/GraphicsSystem.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Assets/AssetCache.h"
#include "Dune/World/World.h"
#include "Dune/Core/ChunkedArray.h"
#include "Dune/Core/FrameContext.h"

namespace Dune
{
	class GameScene;
	struct Camera;

	namespace Graphics
	{
		struct RendererDesc;
	}

	struct FileSystemRoot
	{
		dString name;
		dString path;
	};

	struct ApplicationDesc
	{
		dU32 workerCount{ 3 };
		dU32 physicalDeviceIndex{ 0 };
		dString engineResourcePath;
		dVector<FileSystemRoot> roots;
	};

	struct ViewDesc
	{
		dWString title{ L"Dune" };
		dU32 width{ 1600 };
		dU32 height{ 900 };
		bool enableImGui{ false };
		dString imguiLayoutPath;
		const Graphics::RendererDesc* pRendererDesc{ nullptr };
	};

	struct View
	{
		Window window;
		Graphics::Renderer renderer;
		Graphics::FrameData frameData;
	};

	using ViewHandle = Handle<View>;

	struct RenderRequest
	{
		ViewHandle view;
		World* pWorld{ nullptr };
		Graphics::ViewData viewData;
		dU32 width{ 0 };
		dU32 height{ 0 };
	};

	struct GameFrame
	{
		FrameContext context;
		dVector<RenderRequest> requests;
	};

	class Application
	{
	public:
		void Initialize(const ApplicationDesc& desc);
		void Destroy();

		[[nodiscard]] ViewHandle CreateView(const ViewDesc& desc);
		void DestroyView(ViewHandle view);

		[[nodiscard]] World* CreateWorld(const WorldDesc& desc);
		void DestroyWorld(World* pWorld);

		[[nodiscard]] bool BeginFrame();
		void Update(World& world);
		void Render(ViewHandle view, World& world, const Camera& camera);
		void EndFrame();

		[[nodiscard]] ModelHandle LoadModel(const char* pPath);
		void UnloadModel(ModelHandle model);
		void Spawn(World& world, ModelHandle model);
		[[nodiscard]] ModelHandle LoadModel(World& world, const char* pPath);

		[[nodiscard]] ImageHandle LoadImage(const char* pPath);
		void UnloadImage(ImageHandle image);
		[[nodiscard]] const Image& GetImage(ImageHandle image) const;

		[[nodiscard]] bool IsViewActive(ViewHandle view) const;
		[[nodiscard]] float GetDeltaTime() const { return m_frameContext.deltaTime; }
		[[nodiscard]] Window& GetWindow(ViewHandle view);
		[[nodiscard]] RHI::ImGuiContext& GetImGui(ViewHandle view);
		[[nodiscard]] Graphics::GraphicsSystem& GetGraphics() { return m_graphics; }

	private:
		[[nodiscard]] ImageHandle LoadImage(PathID<Image> source);
		[[nodiscard]] View& GetView(ViewHandle view);
		void RenderFrame(dU64 frameNumber);

	private:
		Graphics::GraphicsSystem m_graphics{};
		AssetCache m_assetCache{};
		HandleAllocator<View> m_viewHandles;
		ChunkedArray<View, 1> m_views;
		dVector<ViewHandle> m_activeViews;
		dVector<World*> m_worlds;
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		FrameContext m_frameContext{};
		GameFrame m_frames[kFrameRingSize];
	};
}
