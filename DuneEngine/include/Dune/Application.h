#pragma once

#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/RenderScene.h"
#include "Dune/Platform/RHI/ImGuiWrapper.h"
#include "Dune/Assets/AssetCache.h"
#include "Dune/Graphics/MeshUtils.h"

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

		[[nodiscard]] bool BeginFrame();
		void Render(ViewHandle view, const GameScene& scene, const Camera& camera);
		void EndFrame();

		[[nodiscard]] ModelHandle LoadModel(const char* pPath);
		void UnloadModel(ModelHandle model);
		void Spawn(GameScene& scene, ModelHandle model);
		[[nodiscard]] ModelHandle LoadModel(GameScene& scene, const char* pPath);

		[[nodiscard]] ImageHandle LoadImage(const char* pPath);
		void UnloadImage(ImageHandle image);
		[[nodiscard]] const Image& GetImage(ImageHandle image) const;

		[[nodiscard]] TextureHandle     CreateTexture(const Graphics::TextureData& data);
		[[nodiscard]] MeshHandle        CreateMesh(const Graphics::MeshData& meshData);
		[[nodiscard]] MaterialHandle    CreateMaterial(const Graphics::MaterialDesc& desc);
		[[nodiscard]] MaterialSetHandle CreateMaterialSet(dSpan<MaterialHandle> materials);

		void DestroyTexture(TextureHandle texture);
		void DestroyMesh(MeshHandle mesh);
		void DestroyMaterial(MaterialHandle material);
		void DestroyMaterialSet(MaterialSetHandle materialSet);

		[[nodiscard]] MeshHandle        GetPrimitive(EPrimitive primitive) const { return m_primitives[(dU32)primitive]; }
		[[nodiscard]] MaterialHandle    GetDefaultMaterial() const { return m_defaultMaterial; }
		[[nodiscard]] MaterialSetHandle GetDefaultMaterialSet() const { return m_defaultMaterialSet; }

		[[nodiscard]] bool IsViewOpen(ViewHandle view) const;
		[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] Window& GetWindow(ViewHandle view);
		[[nodiscard]] Graphics::Renderer& GetRenderer(ViewHandle view);
		[[nodiscard]] RHI::ImGuiWrapper& GetImGui(ViewHandle view);
		[[nodiscard]] Graphics::RenderContext& GetRenderContext() { return m_renderContext; }

	private:
		struct View
		{
			Window window;
			Graphics::Renderer renderer;
			RHI::ImGuiWrapper imgui;
			dU32 resizeEventHandle{ 0 };
			bool hasImGui{ false };
		};

		struct SceneFrame
		{
			const GameScene* pScene{ nullptr };
			Graphics::RenderScene renderScene;
		};

		template<typename T>
		struct PendingRelease
		{
			Handle<T> handle;
			dU64      fenceValue{ 0 };
		};

		void ReleasePendingResources();
		[[nodiscard]] ImageHandle LoadImage(FileSystem::SerializationID<EFileType::Image> source);
		[[nodiscard]] TextureHandle CreateTexture(const ImageData& image);
		[[nodiscard]] Graphics::ModelResources CreateModel(const ModelData& model, dSpan<TextureHandle> images);
		[[nodiscard]] View& GetView(ViewHandle view);
		[[nodiscard]] const Graphics::FrameData& GetFrameData(const GameScene& scene);

	private:
		Graphics::ResourceHandles m_resourceHandles{};
		Graphics::RenderContext m_renderContext{};
		AssetCache m_assetCache{};
		MeshHandle m_primitives[(dU32)EPrimitive::Count]{};
		MaterialHandle m_defaultMaterial{};
		MaterialSetHandle m_defaultMaterialSet{};
		dVector<PendingRelease<Graphics::Texture>>     m_pendingTextures;
		dVector<PendingRelease<Graphics::Mesh>>        m_pendingMeshes;
		dVector<PendingRelease<Graphics::MaterialSet>> m_pendingMaterialSets;
		dVector<std::unique_ptr<View>> m_views;
		dVector<SceneFrame> m_sceneFrames;
		dU32 m_sceneFrameCount{ 0 };
		std::chrono::steady_clock::time_point m_lastFrameTime{};
		float m_deltaTime{ 0.0f };
	};
}
