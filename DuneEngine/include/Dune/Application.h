#pragma once

#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Platform/Window.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Graphics/RHI/ImGuiWrapper.h"
#include "Dune/Asset/AssetCache.h"
#include "Dune/Graphics/MeshUtils.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Component/Transform.h"

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
		void Render(ViewHandle view, const Scene& scene, const Camera& camera);
		void EndFrame();

		[[nodiscard]] ModelHandle LoadModel(const char* pPath);
		void UnloadModel(ModelHandle model);
		void Spawn(Scene& scene, ModelHandle model);
		[[nodiscard]] ModelHandle LoadModel(Scene& scene, const char* pPath);

		[[nodiscard]] ImageHandle LoadImage(const char* pPath);
		void UnloadImage(ImageHandle image);
		[[nodiscard]] const Image& GetImage(ImageHandle image) const;

		[[nodiscard]] Graphics::TextureHandle     CreateTexture(const Graphics::TextureData& data);
		[[nodiscard]] Graphics::MeshHandle        CreateMesh(const Graphics::MeshData& meshData);
		[[nodiscard]] Graphics::MaterialHandle    CreateMaterial(const Graphics::MaterialDesc& desc);
		[[nodiscard]] Graphics::MaterialSetHandle CreateMaterialSet(dSpan<Graphics::MaterialHandle> materials);
		[[nodiscard]] Graphics::ModelResources    CreateModel(const Graphics::ModelData& model, dSpan<Graphics::TextureHandle> images);

		void DestroyTexture(Graphics::TextureHandle texture);
		void DestroyMesh(Graphics::MeshHandle mesh);
		void DestroyMaterial(Graphics::MaterialHandle material);
		void DestroyMaterialSet(Graphics::MaterialSetHandle materialSet);

		EntityID SpawnPrimitive(Scene& scene, EPrimitive primitive, const Transform& transform, Graphics::MaterialSetHandle materials = {});
		[[nodiscard]] Graphics::MeshHandle        GetPrimitive(EPrimitive primitive) const { return m_primitives[(dU32)primitive]; }
		[[nodiscard]] Graphics::MaterialHandle    GetDefaultMaterial() const { return m_defaultMaterial; }
		[[nodiscard]] Graphics::MaterialSetHandle GetDefaultMaterialSet() const { return m_defaultMaterialSet; }

		[[nodiscard]] bool IsViewOpen(ViewHandle view) const;
		[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] Window& GetWindow(ViewHandle view);
		[[nodiscard]] Graphics::Renderer& GetRenderer(ViewHandle view);
		[[nodiscard]] Graphics::ImGuiWrapper& GetImGui(ViewHandle view);
		[[nodiscard]] Graphics::RenderContext& GetRenderContext() { return m_renderContext; }

	private:
		struct View
		{
			Window window;
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

		template<typename T>
		struct PendingRelease
		{
			Handle<T> handle;
			dU64      fenceValue{ 0 };
		};

		void ReleasePendingResources();
		[[nodiscard]] ImageHandle LoadImage(FileSystem::SerializationID<EFileType::Image> source);
		[[nodiscard]] View& GetView(ViewHandle view);
		[[nodiscard]] const Graphics::FrameData& GetFrameData(const Scene& scene);

	private:
		Graphics::ResourceHandles m_resourceHandles{};
		Graphics::RenderContext m_renderContext{};
		AssetCache m_assetCache{};
		Graphics::MeshHandle m_primitives[(dU32)EPrimitive::Count]{};
		Graphics::MaterialHandle m_defaultMaterial{};
		Graphics::MaterialSetHandle m_defaultMaterialSet{};
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
