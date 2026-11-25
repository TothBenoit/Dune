#pragma once

#include "Dune/Graphics/GraphicsSystem.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/WindowSurface.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Assets/AssetCache.h"
#include "Dune/World/World.h"
#include "Dune/Core/FrameContext.h"
#include <thread>
#include <atomic>

namespace Dune
{
	class GameScene;
	class Application;

	namespace Graphics
	{
		struct RendererDesc;
	}

	struct FileSystemRoot
	{
		dString name;
		dString path;
	};

	struct EngineDesc
	{
		dU32 workerCount{ 3 };
		dU32 physicalDeviceIndex{ 0 };
		dString engineResourcePath;
		dVector<FileSystemRoot> roots;
	};

	struct WorldViewDesc
	{
		World* pWorld{ nullptr };
		EntityID camera{};
		Window* pWindow{ nullptr };
		dU32 x{ 0 };
		dU32 y{ 0 };
		dU32 width{ 0 };
		dU32 height{ 0 };
		const Graphics::RendererDesc* pRendererDesc{ nullptr };
	};

	struct WorldView
	{
		World* pWorld{ nullptr };
		EntityID camera{};
		Window* pWindow{ nullptr };
		dU32 x{ 0 };
		dU32 y{ 0 };
		dU32 width{ 0 };
		dU32 height{ 0 };
		Graphics::Renderer renderer;
		Graphics::FrameData frameData;
	};

	void Run(Application& application, const EngineDesc& desc);

	class Engine
	{
	public:
		[[nodiscard]] Window* CreateWindow(const WindowDesc& desc, const Graphics::WindowSurfaceDesc& surfaceDesc = {});
		void DestroyWindow(Window* pWindow);

		[[nodiscard]] WorldView* CreateWorldView(const WorldViewDesc& desc);
		void DestroyWorldView(WorldView* pView);

		[[nodiscard]] World* CreateWorld(const WorldDesc& desc);
		void DestroyWorld(World* pWorld);

		[[nodiscard]] ModelHandle LoadModel(const char* pPath);
		void UnloadModel(ModelHandle model);
		void Spawn(World& world, ModelHandle model);
		[[nodiscard]] ModelHandle LoadModel(World& world, const char* pPath);

		[[nodiscard]] ImageHandle LoadImage(const char* pPath);
		void UnloadImage(ImageHandle image);
		[[nodiscard]] const Image& GetImage(ImageHandle image) const;

		[[nodiscard]] RHI::ImGuiContext& GetImGui(const Window& window);
		[[nodiscard]] Graphics::GraphicsSystem& GetGraphics() { return m_graphics; }

	private:
		friend void Run(Application& application, const EngineDesc& desc);

		Engine() = default;
		void Initialize(const EngineDesc& desc);
		void Destroy();
		void Run(Application& application);

		[[nodiscard]] ImageHandle LoadImage(PathID<Image> source);
		struct WindowEntry
		{
			Window window;
			Graphics::WindowSurface surface;
		};

		struct OSRequest
		{
			void (*pFunction)(void* pData){ nullptr };
			void* pData{ nullptr };
		};

		enum class EOSRequestState : dU8
		{
			Free,
			Claimed,
			Ready,
		};

		[[nodiscard]] WindowEntry& GetWindowEntry(const Window& window);
		void RunOnOSThread(void (*pFunction)(void* pData), void* pData);
		void ProcessOSRequest();
		void PrepareTick();
		void RunTick(Application& application, bool& isRunning);
		void RenderFrame();

	private:
		Graphics::GraphicsSystem m_graphics{};
		AssetCache m_assetCache{};
		dVector<WindowEntry*> m_windows;
		dVector<WorldView*> m_worldViews;
		dVector<World*> m_worlds;
		dVector<Graphics::SurfaceBlit> m_blits;
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		FrameContext m_frameContext{};
		std::thread::id m_osThreadID{};
		OSRequest m_osRequest{};
		std::atomic<EOSRequestState> m_osRequestState{ EOSRequestState::Free };
	};
}
