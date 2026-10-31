#pragma once

#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/RenderScene.h"
#include "Dune/Graphics/MeshUtils.h"
#include "Dune/Core/HandleAllocator.h"
#include "Dune/Core/ChunkedArray.h"

namespace Dune
{
	class Window;
}

namespace Dune::Graphics
{
	class Renderer;

	class GraphicsSystem
	{
	public:
		void Initialize(dU32 physicalDeviceIndex);
		void Destroy();
		void EndFrame();

		void InitializeRenderer(Renderer& renderer, Window& window, const RendererDesc& desc);
		void Render(Renderer& renderer, RenderSceneHandle scene, const ViewData& view, float deltaTime, FrameData& frameData);

		[[nodiscard]] TextureHandle     CreateTexture(const TextureData& data);
		[[nodiscard]] MeshHandle        CreateMesh(const MeshData& meshData);
		[[nodiscard]] MaterialHandle    CreateMaterial(const MaterialDesc& desc);
		[[nodiscard]] MaterialSetHandle CreateMaterialSet(dSpan<MaterialHandle> materials);

		void DestroyTexture(TextureHandle texture);
		void DestroyMesh(MeshHandle mesh);
		void DestroyMaterial(MaterialHandle material);
		void DestroyMaterialSet(MaterialSetHandle materialSet);

		void CreateImageTexture(ImageHandle image, const ImageData& data);
		void ReleaseImageTexture(ImageHandle image);
		[[nodiscard]] TextureHandle GetImageTexture(ImageHandle image) const;

		const ModelResources& CreateModelResources(ModelHandle model, const ModelData& data, dSpan<TextureHandle> textures);
		void ReleaseModelResources(ModelHandle model);
		[[nodiscard]] const ModelResources& GetModelResources(ModelHandle model) const;

		[[nodiscard]] RenderSceneHandle CreateRenderScene();
		void DestroyRenderScene(RenderSceneHandle scene);
		[[nodiscard]] RenderScene& GetRenderScene(RenderSceneHandle scene);

		[[nodiscard]] MeshHandle        GetPrimitive(EPrimitive primitive) const { return m_primitives[(dU32)primitive]; }
		[[nodiscard]] MaterialHandle    GetDefaultMaterial() const { return m_defaultMaterial; }
		[[nodiscard]] MaterialSetHandle GetDefaultMaterialSet() const { return m_defaultMaterialSet; }
		[[nodiscard]] MaterialKey       GetMaterialKey(MaterialHandle material) const;
		[[nodiscard]] RenderContext&    GetRenderContext() { return m_renderContext; }

	private:
		template<typename T>
		struct PendingRelease
		{
			Handle<T> handle;
			dU64      fenceValue{ 0 };
		};

		[[nodiscard]] TextureHandle CreateTexture(const ImageData& image);
		[[nodiscard]] const GPUResourceTable& GetResourceTable();
		void ReleasePendingResources();

	private:
		ResourceCache                           m_resourceCache{};
		RenderContext                           m_renderContext{};
		HandleAllocator<RenderScene>            m_renderSceneHandles;
		ChunkedArray<RenderScene, 1>            m_renderScenes;
		GPUResourceTable                        m_resourceTable{};
		bool                                    m_resourceTableDirty{ true };
		dU32                                    m_drawResourcesVersion{ 0 };
		MeshHandle                              m_primitives[(dU32)EPrimitive::Count]{};
		MaterialHandle                          m_defaultMaterial{};
		MaterialSetHandle                       m_defaultMaterialSet{};
		dVector<PendingRelease<Texture>>        m_pendingTextures;
		dVector<PendingRelease<Mesh>>           m_pendingMeshes;
		dVector<PendingRelease<MaterialSet>>    m_pendingMaterialSets;
	};
}
