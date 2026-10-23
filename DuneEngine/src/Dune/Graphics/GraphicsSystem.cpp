#include "pch.h"
#include "Dune/Graphics/GraphicsSystem.h"
#include "Dune/Graphics/Renderer.h"

namespace Dune::Graphics
{
	void GraphicsSystem::Initialize()
	{
		m_renderContext.Initialize(m_handles);
		m_renderScenes.Initialize(m_renderSceneHandles);
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
		m_isResourceTableValid = false;
	}

	void GraphicsSystem::InitializeRenderer(Renderer& renderer, Window& window)
	{
		m_renderContext.InitializeRenderer(renderer, window);
	}

	void GraphicsSystem::Render(Renderer& renderer, RenderSceneHandle scene, const ViewData& view, float deltaTime, FrameData& frameData)
	{
		m_renderContext.GetUploadContext().Submit();
		const RenderScene& renderScene = m_renderScenes.Get(scene);
		frameData.pResources = &GetResourceTable();
		frameData.lights = dSpan<const LightData>(renderScene.GetLights());
		BuildDrawItems(renderScene, frameData.drawItems);
		frameData.deltaTime = deltaTime;
		renderer.Render(frameData, view);
	}

	TextureHandle GraphicsSystem::CreateTexture(const TextureData& data)
	{
		const TextureHandle handle = m_handles.textures.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, data);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Texture::kUploadAlignment);
		resources.Upload(handle, data, uploadContext.GetCommandList(), staging);
		return handle;
	}

	TextureHandle GraphicsSystem::CreateTexture(const ImageData& image)
	{
		const TextureHandle handle = m_handles.textures.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateTexture(handle, image);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Texture::kUploadAlignment);
		resources.Upload(handle, image, uploadContext.GetCommandList(), staging);
		return handle;
	}

	MeshHandle GraphicsSystem::CreateMesh(const MeshData& meshData)
	{
		const MeshHandle handle = m_handles.meshes.Allocate();
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateMesh(handle, meshData);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handle), Mesh::kUploadAlignment);
		resources.Upload(handle, meshData, uploadContext.GetCommandList(), staging);
		return handle;
	}

	MaterialHandle GraphicsSystem::CreateMaterial(const MaterialDesc& desc)
	{
		const MaterialHandle handle = m_handles.materials.Allocate();
		m_renderContext.GetResourceManager().CreateMaterial(handle, desc);
		return handle;
	}

	MaterialSetHandle GraphicsSystem::CreateMaterialSet(dSpan<MaterialHandle> materials)
	{
		const MaterialSetHandle handle = m_handles.materialSets.Allocate();
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
		m_handles.materials.Free(material);
	}

	void GraphicsSystem::DestroyMaterialSet(MaterialSetHandle materialSet)
	{
		m_pendingMaterialSets.push_back({ .handle = materialSet, .fenceValue = m_renderContext.GetNextFenceValue() });
	}

	void GraphicsSystem::CreateImageTexture(ImageHandle image, const ImageData& data)
	{
		Assert(m_handles.imageTextures.find(image) == m_handles.imageTextures.end());
		m_handles.imageTextures.emplace(image, CreateTexture(data));
	}

	void GraphicsSystem::ReleaseImageTexture(ImageHandle image)
	{
		auto it = m_handles.imageTextures.find(image);
		Assert(it != m_handles.imageTextures.end());
		DestroyTexture(it->second);
		m_handles.imageTextures.erase(it);
	}

	TextureHandle GraphicsSystem::GetImageTexture(ImageHandle image) const
	{
		return m_handles.GetTexture(image);
	}

	const ModelResources& GraphicsSystem::CreateModelResources(ModelHandle model, const ModelData& data, dSpan<TextureHandle> textures)
	{
		const ModelResources handles = m_handles.AllocateModel(data);
		ResourceManager& resources = m_renderContext.GetResourceManager();
		UploadContext& uploadContext = m_renderContext.GetUploadContext();
		resources.CreateModel(handles, data, textures);
		const RHI::StagingMemory staging = uploadContext.AllocateStaging(resources.GetUploadSize(handles), Mesh::kUploadAlignment);
		resources.Upload(handles, data, uploadContext.GetCommandList(), staging);
		return m_handles.modelResources.emplace(model, handles).first->second;
	}

	void GraphicsSystem::ReleaseModelResources(ModelHandle model)
	{
		auto it = m_handles.modelResources.find(model);
		Assert(it != m_handles.modelResources.end());
		const ModelResources modelResources = std::move(it->second);
		m_handles.modelResources.erase(it);

		for (MaterialSetHandle materialSet : modelResources.nodeMaterialSets)
			DestroyMaterialSet(materialSet);
		for (MaterialHandle material : modelResources.materials)
			DestroyMaterial(material);
		for (MeshHandle mesh : modelResources.meshes)
			DestroyMesh(mesh);
	}

	const ModelResources& GraphicsSystem::GetModelResources(ModelHandle model) const
	{
		return m_handles.GetModelResources(model);
	}

	RenderSceneHandle GraphicsSystem::CreateRenderScene()
	{
		const RenderSceneHandle handle = m_renderSceneHandles.Allocate();
		m_renderScenes.Insert(handle, RenderScene{});
		return handle;
	}

	void GraphicsSystem::DestroyRenderScene(RenderSceneHandle scene)
	{
		m_renderScenes.Remove(scene);
		m_renderSceneHandles.Free(scene);
	}

	RenderScene& GraphicsSystem::GetRenderScene(RenderSceneHandle scene)
	{
		return m_renderScenes.Get(scene);
	}

	MaterialKey GraphicsSystem::GetMaterialKey(MaterialHandle material) const
	{
		return m_renderContext.GetResourceManager().GetMaterial(material).GetKey();
	}

	const GPUResourceTable& GraphicsSystem::GetResourceTable()
	{
		if (!m_isResourceTableValid)
		{
			m_renderContext.GetResourceManager().FillResourceTable(m_resourceTable);
			m_isResourceTableValid = true;
		}
		return m_resourceTable;
	}

	void GraphicsSystem::BuildDrawItems(const RenderScene& scene, dVector<DrawItem>& outDrawItems) const
	{
		const ResourceManager& resources = m_renderContext.GetResourceManager();
		outDrawItems.clear();
		for (const RenderObject& object : scene.GetObjects())
		{
			const Mesh& mesh = resources.GetMesh(object.mesh);
			const MaterialSet& materialSet = resources.GetMaterialSet(object.materials);
			Assert(materialSet.materials.size() >= mesh.GetMaterialSlotCount());
			for (const SubMesh& subMesh : mesh.GetSubMeshes())
			{
				const MaterialHandle materialHandle = materialSet.materials[subMesh.materialSlot];
				outDrawItems.push_back(
					{
						.objectToWorld = object.objectToWorld,
						.meshIdx = object.mesh.GetIndex(),
						.materialIdx = materialHandle.GetIndex(),
						.indexOffset = subMesh.indexOffset,
						.indexCount = subMesh.indexCount,
						.vertexOffset = subMesh.vertexOffset,
						.materialKey = resources.GetMaterial(materialHandle).GetKey(),
					});
			}
		}
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
		ReleaseCompleted(m_pendingTextures, completedValue, [&](TextureHandle texture) { resources.Destroy(texture); m_handles.textures.Free(texture); });
		ReleaseCompleted(m_pendingMeshes, completedValue, [&](MeshHandle mesh) { resources.Destroy(mesh); m_handles.meshes.Free(mesh); });
		ReleaseCompleted(m_pendingMaterialSets, completedValue, [&](MaterialSetHandle materialSet) { resources.Destroy(materialSet); m_handles.materialSets.Free(materialSet); });
	}
}
