#include "pch.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/CommandList.h"

namespace Dune::Graphics
{
	static StagingMemory SubAllocate(const StagingMemory& staging, dU64& offset, dU64 byteSize, dU64 alignment)
	{
		offset = MemoryUtils::AlignUp(offset, alignment);
		const StagingMemory subRegion{ .buffer = staging.buffer, .offset = staging.offset + offset, .pCpuAddress = (dU8*)staging.pCpuAddress + offset };
		offset += byteSize;
		return subRegion;
	}

	static MeshDesc ToMeshDesc(const MeshData& meshData)
	{
		return
		{
			.pVertices = meshData.vertices.data(),
			.vertexCount = (dU32)meshData.vertices.size(),
			.vertexByteStride = sizeof(Vertex),
			.pIndices = meshData.indices.data(),
			.indexCount = (dU32)meshData.indices.size(),
			.isIndex32bits = true,
			.pSubMeshes = meshData.subMeshes.data(),
			.subMeshCount = (dU32)meshData.subMeshes.size(),
			.materialSlotCount = meshData.materialSlotCount
		};
	}

	static MeshDesc ToMeshDesc(const ModelMesh& meshData)
	{
		return
		{
			.pVertices = meshData.vertices.data(),
			.vertexCount = (dU32)meshData.vertices.size(),
			.vertexByteStride = sizeof(Vertex),
			.pIndices = meshData.indices.data(),
			.indexCount = (dU32)meshData.indices.size(),
			.isIndex32bits = true,
			.pSubMeshes = meshData.subMeshes.data(),
			.subMeshCount = (dU32)meshData.subMeshes.size(),
			.materialSlotCount = meshData.materialSlotCount
		};
	}

	ModelResources ResourceCache::AllocateModel(const ModelData& model)
	{
		ModelResources resources;
		for (dU32 i = 0; i < (dU32)model.materials.size(); i++)
			resources.materials.push_back(materials.Allocate());
		for (dU32 i = 0; i < (dU32)model.meshes.size(); i++)
			resources.meshes.push_back(meshes.Allocate());
		for (dU32 i = 0; i < (dU32)model.nodes.size(); i++)
			resources.nodeMaterialSets.push_back(materialSets.Allocate());
		return resources;
	}

	TextureHandle ResourceCache::GetTexture(ImageHandle image) const
	{
		auto it = imageTextures.find(image);
		return it != imageTextures.end() ? it->second : TextureHandle{};
	}

	const ModelResources& ResourceCache::GetModelResources(ModelHandle model) const
	{
		auto it = modelResources.find(model);
		Assert(it != modelResources.end());
		return it->second;
	}

	void ResourceManager::Initialize(RHI::Device& device, const ResourceCache& resourceCache, DescriptorAllocator& srvAllocator)
	{
		m_pDevice = &device;
		m_pSRVAllocator = &srvAllocator;
		m_textures.Initialize(resourceCache.textures);
		m_materials.Initialize(resourceCache.materials);
		m_materialSets.Initialize(resourceCache.materialSets);
		m_meshes.Initialize(resourceCache.meshes);
	}

	void ResourceManager::Destroy()
	{
		for (dU32 i = 0; i < m_textures.GetCapacity(); i++)
			if (m_textures.IsAliveAt(i))
				Destroy(m_textures.GetHandleAt(i));
		for (dU32 i = 0; i < m_meshes.GetCapacity(); i++)
			if (m_meshes.IsAliveAt(i))
				Destroy(m_meshes.GetHandleAt(i));

		m_pSRVAllocator = nullptr;
		m_pDevice = nullptr;
	}

	void ResourceManager::CreateTexture(TextureHandle handle, const TextureData& data)
	{
		Texture texture{};
		texture.Initialize(*m_pDevice, *m_pSRVAllocator, { .format = data.format, .width = data.width, .height = data.height, .mipLevels = data.mipLevels });
		m_textures.Insert(handle, std::move(texture));
	}

	void ResourceManager::CreateTexture(TextureHandle handle, const ImageData& image)
	{
		Texture texture{};
		texture.Initialize(*m_pDevice, *m_pSRVAllocator, { .format = image.format, .width = image.width, .height = image.height, .mipLevels = image.mipLevels });
		m_textures.Insert(handle, std::move(texture));
	}

	dU32 ResourceManager::GetViewIndex(TextureHandle handle) const
	{
		if (!handle.IsValid())
			return dU32(-1);
		return m_pSRVAllocator->GetHeap().GetIndex(m_textures.Get(handle).GetView());
	}

	dU32 ResourceManager::ResolveSRGBViewIndex(TextureHandle handle)
	{
		if (!handle.IsValid())
			return dU32(-1);
		return m_pSRVAllocator->GetHeap().GetIndex(m_textures.Get(handle).ResolveSRGBView(*m_pDevice, *m_pSRVAllocator));
	}

	void ResourceManager::CreateMaterial(MaterialHandle handle, const MaterialDesc& desc)
	{
		m_materials.Insert(handle,
		{
			.shaderData =
			{
				.baseColor = desc.baseColor,
				.metalnessFactor = desc.metalness,
				.roughnessFactor = desc.roughness,
				.albedoIdx = ResolveSRGBViewIndex(desc.albedo),
				.normalIdx = GetViewIndex(desc.normal),
				.roughnessMetalnessIdx = GetViewIndex(desc.roughnessMetalness),
				.alphaCutoff = desc.alphaCutoff
			},
			.alphaMode = desc.alphaMode,
			.faceCulling = desc.faceCulling,
		});
	}

	void ResourceManager::CreateMaterialSet(MaterialSetHandle handle, dSpan<MaterialHandle> materials)
	{
		for (MaterialHandle material : materials)
			Assert(m_materials.IsValid(material));
		m_materialSets.Insert(handle, { .materials = dVector<MaterialHandle>(materials.begin(), materials.end()) });
	}

	void ResourceManager::CreateMesh(MeshHandle handle, const MeshData& meshData)
	{
		CreateMesh(handle, ToMeshDesc(meshData));
	}

	void ResourceManager::CreateMesh(MeshHandle handle, const MeshDesc& desc)
	{
		Mesh mesh{};
		mesh.Initialize(*m_pDevice, desc);
		m_meshes.Insert(handle, std::move(mesh));
	}

	void ResourceManager::CreateModel(const ModelResources& handles, const ModelData& model, dSpan<TextureHandle> images)
	{
		Assert(images.GetSize() == model.images.size());
		const auto ToTexture = [&](dU32 imageIndex) { return imageIndex != kInvalidModelIndex ? images[imageIndex] : TextureHandle{}; };
		for (dU32 i = 0; i < (dU32)model.materials.size(); i++)
		{
			const ModelMaterial& material = model.materials[i];
			CreateMaterial(handles.materials[i],
				{
					.baseColor = material.baseColor,
					.metalness = material.metalness,
					.roughness = material.roughness,
					.alphaCutoff = material.alphaCutoff,
					.alphaMode = material.alphaMode,
					.faceCulling = material.faceCulling,
					.albedo = ToTexture(material.albedoImage),
					.normal = ToTexture(material.normalImage),
					.roughnessMetalness = ToTexture(material.roughnessMetalnessImage),
				});
		}

		for (dU32 i = 0; i < (dU32)model.meshes.size(); i++)
			CreateMesh(handles.meshes[i], ToMeshDesc(model.meshes[i]));

		dVector<MaterialHandle> nodeMaterials;
		for (dU32 i = 0; i < (dU32)model.nodes.size(); i++)
		{
			nodeMaterials.clear();
			for (dU32 materialIndex : model.nodes[i].materials)
				nodeMaterials.push_back(handles.materials[materialIndex]);
			CreateMaterialSet(handles.nodeMaterialSets[i], nodeMaterials);
		}
	}

	dU64 ResourceManager::GetUploadSize(TextureHandle handle) const
	{
		return m_textures.Get(handle).GetUploadSize();
	}

	dU64 ResourceManager::GetUploadSize(MeshHandle handle) const
	{
		return m_meshes.Get(handle).GetUploadSize();
	}

	dU64 ResourceManager::GetUploadSize(const ModelResources& resources) const
	{
		dU64 byteSize = 0;
		for (MeshHandle mesh : resources.meshes)
			byteSize = MemoryUtils::AlignUp(byteSize, Mesh::kUploadAlignment) + GetUploadSize(mesh);
		return byteSize;
	}

	void ResourceManager::Upload(const ModelResources& resources, const ModelData& model, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		Assert(staging.IsValid() && staging.offset % Mesh::kUploadAlignment == 0);
		dU64 offset = 0;
		for (dU32 i = 0; i < (dU32)resources.meshes.size(); i++)
			Upload(resources.meshes[i], ToMeshDesc(model.meshes[i]), commandList, SubAllocate(staging, offset, GetUploadSize(resources.meshes[i]), Mesh::kUploadAlignment));
	}

	void ResourceManager::Upload(TextureHandle handle, const TextureData& data, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		m_textures.Get(handle).Upload(data.bytes.data(), commandList, staging);
	}

	void ResourceManager::Upload(TextureHandle handle, const ImageData& image, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		m_textures.Get(handle).Upload(image.bytes.data(), commandList, staging);
	}

	void ResourceManager::Upload(MeshHandle handle, const MeshData& meshData, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		Upload(handle, ToMeshDesc(meshData), commandList, staging);
	}

	void ResourceManager::Upload(MeshHandle handle, const MeshDesc& desc, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		Mesh& mesh = m_meshes.Get(handle);
		mesh.Upload(desc, commandList, staging);
		commandList.Transition(mesh.GetIndexBuffer(), RHI::EResourceState::CopyDest, RHI::EResourceState::IndexBuffer);
		commandList.Transition(mesh.GetVertexBuffer(), RHI::EResourceState::CopyDest, RHI::EResourceState::VertexBuffer);
	}

	static GPUMeshView ToMeshView(const Mesh& mesh)
	{
		const RHI::Buffer& indexBuffer = mesh.GetIndexBuffer();
		const RHI::Buffer& vertexBuffer = mesh.GetVertexBuffer();
		return
		{
			.indicesGPUAddress = indexBuffer.GetGPUAddress(),
			.indicesByteSize = indexBuffer.GetByteSize(),
			.indicesAre32Bit = mesh.IsIndex32bits(),
			.verticesGPUAddress = vertexBuffer.GetGPUAddress(),
			.verticesByteSize = vertexBuffer.GetByteSize(),
			.verticesByteStride = mesh.GetVertexByteStride(),
		};
	}

	void ResourceManager::FillResourceTable(GPUResourceTable& table) const
	{
		table.meshes.assign(m_meshes.GetCapacity(), GPUMeshView{});
		for (dU32 i = 0; i < m_meshes.GetCapacity(); i++)
			if (m_meshes.IsAliveAt(i))
				table.meshes[i] = ToMeshView(m_meshes.GetAt(i));

		table.materials.assign(m_materials.GetCapacity(), MaterialData{});
		for (dU32 i = 0; i < m_materials.GetCapacity(); i++)
			if (m_materials.IsAliveAt(i))
				table.materials[i] = m_materials.GetAt(i).shaderData;
	}

	void ResourceManager::Destroy(TextureHandle handle)
	{
		m_textures.Remove(handle).Destroy(*m_pSRVAllocator);
	}

	void ResourceManager::Destroy(MaterialHandle handle)
	{
		m_materials.Remove(handle);
	}

	void ResourceManager::Destroy(MaterialSetHandle handle)
	{
		m_materialSets.Remove(handle);
	}

	void ResourceManager::Destroy(MeshHandle handle)
	{
		m_meshes.Remove(handle).Destroy();
	}
}
