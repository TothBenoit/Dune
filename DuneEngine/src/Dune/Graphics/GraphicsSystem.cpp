#include "pch.h"
#include "Dune/Graphics/GraphicsSystem.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void GraphicsSystem::Initialize(dU32 physicalDeviceIndex)
	{
		m_renderContext.Initialize(m_resourceCache, physicalDeviceIndex);
		for (dU32 primitive = 0; primitive < (dU32)EPrimitive::Count; primitive++)
			m_primitives[primitive] = CreateMesh(MeshUtils::CreatePrimitive((EPrimitive)primitive));
		m_defaultMaterial = CreateMaterial({});
		m_defaultMaterialSet = CreateMaterialSet({ m_defaultMaterial });
	}

	void GraphicsSystem::Destroy()
	{
		m_pendingTextures.clear();
		m_pendingMeshes.clear();
		m_pendingMaterialSets.clear();
		m_renderScenes.Destroy();
		m_renderSceneHandles.Destroy();
		m_renderContext.Destroy();
	}

	void GraphicsSystem::EndFrame()
	{
		m_renderContext.GetUploadContext().Submit();
		m_renderContext.Signal();
		ReleasePendingResources();
	}

	void GraphicsSystem::InitializeRenderer(Renderer& renderer, Window& window, const RendererDesc& desc)
	{
		m_renderContext.InitializeRenderer(renderer, window, desc);
	}

	void GraphicsSystem::Render(Renderer& renderer, RenderSceneHandle scene, const ViewData& view, const FrameContext& frameContext, FrameData& frameData)
	{
		m_renderContext.GetUploadContext().Submit();
		RenderScene& renderScene = GetRenderScene(scene);
		const ResourceManager& resources = m_renderContext.GetResourceManager();
		frameData.pResources = &resources.GetResourceTable();
		frameData.lights = dSpan<const LightData>(renderScene.GetLightsData());
		frameData.drawItems = dSpan<const DrawItem>(renderScene.GetDrawItems(resources));
		renderer.Render(frameContext, frameData, view);
	}

	TextureHandle GraphicsSystem::CreateTexture(const TextureData& data)
	{
		const TextureHandle handle = m_resourceCache.textures.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, data);
		const StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Texture::kUploadAlignment);
		resources.Upload(handle, data, uploadContext.GetCommandList(), staging);
		return handle;
	}

	TextureHandle GraphicsSystem::CreateTexture(const ImageData& image)
	{
		const TextureHandle handle = m_resourceCache.textures.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, image);
		const StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Texture::kUploadAlignment);
		resources.Upload(handle, image, uploadContext.GetCommandList(), staging);
		return handle;
	}

	MeshHandle GraphicsSystem::CreateMesh(const MeshData& meshData)
	{
		const MeshHandle handle = m_resourceCache.meshes.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateMesh(handle, meshData);
		const StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Mesh::kUploadAlignment);
		resources.Upload(handle, meshData, uploadContext.GetCommandList(), staging);
		return handle;
	}

	MaterialHandle GraphicsSystem::CreateMaterial(const MaterialDesc& desc)
	{
		const MaterialHandle handle = m_resourceCache.materials.Allocate();
		m_renderContext.GetResourceManager().CreateMaterial(handle, desc);
		return handle;
	}

	MaterialSetHandle GraphicsSystem::CreateMaterialSet(dSpan<MaterialHandle> materials)
	{
		const MaterialSetHandle handle = m_resourceCache.materialSets.Allocate();
		m_renderContext.GetResourceManager().CreateMaterialSet(handle, materials);
		return handle;
	}

	void GraphicsSystem::DestroyTexture(TextureHandle texture)
	{
		m_pendingTextures.push_back({ .handle = texture, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void GraphicsSystem::DestroyMesh(MeshHandle mesh)
	{
		m_pendingMeshes.push_back({ .handle = mesh, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void GraphicsSystem::DestroyMaterial(MaterialHandle material)
	{
		m_renderContext.GetResourceManager().Destroy(material);
		m_resourceCache.materials.Free(material);
	}

	void GraphicsSystem::DestroyMaterialSet(MaterialSetHandle materialSet)
	{
		m_pendingMaterialSets.push_back({ .handle = materialSet, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void GraphicsSystem::CreateImageTexture(ImageHandle image, const ImageData& data)
	{
		Assert(m_resourceCache.imageTextures.find(image) == m_resourceCache.imageTextures.end());
		m_resourceCache.imageTextures.emplace(image, CreateTexture(data));
	}

	void GraphicsSystem::ReleaseImageTexture(ImageHandle image)
	{
		auto it = m_resourceCache.imageTextures.find(image);
		Assert(it != m_resourceCache.imageTextures.end());
		DestroyTexture(it->second);
		m_resourceCache.imageTextures.erase(it);
	}

	TextureHandle GraphicsSystem::GetImageTexture(ImageHandle image) const
	{
		return m_resourceCache.GetTexture(image);
	}

	const ModelResources& GraphicsSystem::CreateModelResources(ModelHandle model, const ModelData& data, dSpan<TextureHandle> textures)
	{
		const ModelResources handles = m_resourceCache.AllocateModel(data);
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateModel(handles, data, textures);
		const StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handles), Mesh::kUploadAlignment);
		resources.Upload(handles, data, uploadContext.GetCommandList(), staging);
		return m_resourceCache.modelResources.emplace(model, handles).first->second;
	}

	void GraphicsSystem::ReleaseModelResources(ModelHandle model)
	{
		auto it = m_resourceCache.modelResources.find(model);
		Assert(it != m_resourceCache.modelResources.end());
		const ModelResources modelResources = std::move(it->second);
		m_resourceCache.modelResources.erase(it);

		for (MaterialSetHandle materialSet : modelResources.nodeMaterialSets)
			DestroyMaterialSet(materialSet);
		for (MaterialHandle material : modelResources.materials)
			DestroyMaterial(material);
		for (MeshHandle mesh : modelResources.meshes)
			DestroyMesh(mesh);
	}

	const ModelResources& GraphicsSystem::GetModelResources(ModelHandle model) const
	{
		return m_resourceCache.GetModelResources(model);
	}

	RenderSceneHandle GraphicsSystem::CreateRenderScene()
	{
		const RenderSceneHandle handle = m_renderSceneHandles.Allocate();
		m_renderScenes.Resolve(handle.GetIndex()).Initialize();
		return handle;
	}

	void GraphicsSystem::DestroyRenderScene(RenderSceneHandle scene)
	{
		GetRenderScene(scene).Destroy();
		m_renderSceneHandles.Free(scene);
	}

	RenderScene& GraphicsSystem::GetRenderScene(RenderSceneHandle scene)
	{
		Assert(m_renderSceneHandles.IsValid(scene));
		return m_renderScenes[scene.GetIndex()];
	}

	MaterialKey GraphicsSystem::GetMaterialKey(MaterialHandle material) const
	{
		return m_renderContext.GetResourceManager().GetMaterial(material).GetKey();
	}

	template<typename TPending, typename TRelease>
	static void ReleaseCompleted(dVector<TPending>& pending, dU64 completedValue, TRelease release)
	{
		for (dU32 i = 0; i < (dU32)pending.size();)
		{
			if (pending[i].fenceValue <= completedValue)
			{
				release(pending[i].handle);
				pending[i] = pending.back();
				pending.pop_back();
			}
			else
				i++;
		}
	}

	void GraphicsSystem::ReleasePendingResources()
	{
		ResourceManager& resources = m_renderContext.GetResourceManager();
		const dU64 completedValue = m_renderContext.GetCompletedFenceValue();
		ReleaseCompleted(m_pendingTextures, completedValue, [&](TextureHandle texture) { resources.Destroy(texture); m_resourceCache.textures.Free(texture); });
		ReleaseCompleted(m_pendingMeshes, completedValue, [&](MeshHandle mesh) { resources.Destroy(mesh); m_resourceCache.meshes.Free(mesh); });
		ReleaseCompleted(m_pendingMaterialSets, completedValue, [&](MaterialSetHandle materialSet) { resources.Destroy(materialSet); m_resourceCache.materialSets.Free(materialSet); });
	}
}
