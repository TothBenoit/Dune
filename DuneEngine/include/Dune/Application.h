#pragma once

#include "Dune/Graphics/GraphicsSystem.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Platform/RHI/ImGuiWrapper.h"
#include "Dune/Assets/AssetCache.h"
#include "Dune/World/World.h"
#include "Dune/Core/ChunkedArray.h"

namespace Dune
{
	class GameScene;
	struct Camera;

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

		[[nodiscard]] WorldHandle CreateWorld(const WorldDesc& desc);
		void DestroyWorld(WorldHandle world);
		[[nodiscard]] World& GetWorld(WorldHandle world);

		[[nodiscard]] bool BeginFrame();
		void Update(WorldHandle world);
		void Render(ViewHandle view, WorldHandle world, const Camera& camera);
		void EndFrame();

		[[nodiscard]] ModelHandle LoadModel(const char* pPath);
		void UnloadModel(ModelHandle model);
		void Spawn(WorldHandle world, ModelHandle model);
		[[nodiscard]] ModelHandle LoadModel(WorldHandle world, const char* pPath);

		[[nodiscard]] ImageHandle LoadImage(const char* pPath);
		void UnloadImage(ImageHandle image);
		[[nodiscard]] const Image& GetImage(ImageHandle image) const;

		[[nodiscard]] bool IsViewOpen(ViewHandle view) const;
		[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] Window& GetWindow(ViewHandle view);
		[[nodiscard]] Graphics::Renderer& GetRenderer(ViewHandle view);
		[[nodiscard]] RHI::ImGuiWrapper& GetImGui(ViewHandle view);
		[[nodiscard]] Graphics::GraphicsSystem& GetGraphics() { return m_graphics; }

	private:
		struct View
		{
			Window window;
			Graphics::Renderer renderer;
			RHI::ImGuiWrapper imgui;
			Graphics::FrameData frameData;
			dU32 resizeEventHandle{ 0 };
			bool hasImGui{ false };
		};

		[[nodiscard]] ImageHandle LoadImage(FileSystem::SerializationID<EFileType::Image> source);
		[[nodiscard]] View& GetView(ViewHandle view);

	private:
		Graphics::GraphicsSystem m_graphics{};
		AssetCache m_assetCache{};
		dVector<std::unique_ptr<View>> m_views;
		HandleAllocator<World> m_worldHandles;
		ChunkedArray<World> m_worlds;
		dU32 m_worldCapacity{ 0 };
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		float m_deltaTime{ 0.0f };
	};
}
