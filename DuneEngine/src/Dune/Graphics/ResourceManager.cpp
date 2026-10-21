#include "pch.h"
#include "Dune/Graphics/ResourceManager.h"
#include "Dune/Graphics/FrameData.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/CommandList.h"

namespace Dune::Graphics
{
	static RHI::StagingMemory SubAllocate(const RHI::StagingMemory& staging, dU64& offset, dU64 byteSize, dU64 alignment)
	{
		offset = MemoryUtils::AlignUp(offset, alignment);
		const RHI::StagingMemory subRegion{ .buffer = staging.buffer, .offset = staging.offset + offset, .pCpuAddress = (dU8*)staging.pCpuAddress + offset };
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

	ModelResources ResourceHandles::AllocateModel(const ModelData& model)
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

	void ResourceManager::Initialize(RHI::Device& device, const ResourceHandles& handles)
	{
		m_pDevice = &device;
		m_textures.Initialize(handles.textures);
		m_materials.Initialize(handles.materials);
		m_materialSets.Initialize(handles.materialSets);
		m_meshes.Initialize(handles.meshes);
		m_srvHeap.Initialize(device, { .type = RHI::EDescriptorHeapType::SRV_CBV_UAV, .capacity = kSharedSRVCapacity, .isShaderVisible = false });
	}

	void ResourceManager::Destroy()
	{
		for (dU32 i = 0; i < m_textures.GetCapacity(); i++)
			if (m_textures.IsAliveAt(i))
				Destroy(m_textures.GetHandleAt(i));
		for (dU32 i = 0; i < m_meshes.GetCapacity(); i++)
			if (m_meshes.IsAliveAt(i))
				Destroy(m_meshes.GetHandleAt(i));

		m_srvHeap.Destroy();
		m_pDevice = nullptr;
	}

	void ResourceManager::CreateTexture(TextureHandle handle, const RHI::TextureData& image)
	{
		RHI::Texture texture{};
		texture.Initialize(*m_pDevice,
			{
				.usage = RHI::ETextureUsage::ShaderResource,
				.dimensions = { image.width, image.height, 1 },
				.mipLevels = image.mipLevels,
				.format = RHI::ToTypeless(image.format),
			});

		const RHI::Descriptor srv = m_srvHeap.Allocate();
		m_pDevice->CreateSRV(srv, texture, { .mipLevels = image.mipLevels, .format = image.format });

		RHI::Descriptor srgbSRV = srv;
		const RHI::EFormat srgbFormat = RHI::ToSRGB(image.format);
		if (srgbFormat != image.format)
		{
			srgbSRV = m_srvHeap.Allocate();
			m_pDevice->CreateSRV(srgbSRV, texture, { .mipLevels = image.mipLevels, .format = srgbFormat });
		}
		m_textures.Insert(handle, { .texture = texture, .srv = srv, .srgbSRV = srgbSRV });
	}

	dU32 ResourceManager::GetBindlessIndex(TextureHandle handle, bool isSRGB) const
	{
		if (!handle.IsValid())
			return dU32(-1);
		const TextureEntry& entry = m_textures.Get(handle);
		return m_srvHeap.GetIndex(isSRGB ? entry.srgbSRV : entry.srv);
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
				.albedoIdx = GetBindlessIndex(desc.albedo, true),
				.normalIdx = GetBindlessIndex(desc.normal, false),
				.roughnessMetalnessIdx = GetBindlessIndex(desc.roughnessMetalness, false),
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
		Mesh mesh{};
		mesh.Initialize(*m_pDevice, ToMeshDesc(meshData));
		m_meshes.Insert(handle, std::move(mesh));
	}

	void ResourceManager::CreateModel(const ModelResources& handles, const ModelData& model, dSpan<TextureHandle> images)
	{
		Assert(images.GetSize() == model.images.size());
		const auto ToTexture = [&](dU32 imageIndex) { return imageIndex != kInvalidModelIndex ? images[imageIndex] : TextureHandle{}; };
		for (dU32 i = 0; i < (dU32)model.materials.size(); i++)
		{
			const ModelMaterial& material = model.materials[i];
			MaterialDesc desc{ material.desc };
			desc.albedo = ToTexture(material.albedoImage);
			desc.normal = ToTexture(material.normalImage);
			desc.roughnessMetalness = ToTexture(material.roughnessMetalnessImage);
			CreateMaterial(handles.materials[i], desc);
		}

		for (dU32 i = 0; i < (dU32)model.meshes.size(); i++)
			CreateMesh(handles.meshes[i], model.meshes[i]);

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
		return m_textures.Get(handle).texture.GetUploadSize();
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

	void ResourceManager::Upload(const ModelResources& resources, const ModelData& model, RHI::CommandList& commandList, const RHI::StagingMemory& staging)
	{
		Assert(staging.IsValid() && staging.offset % Mesh::kUploadAlignment == 0);
		dU64 offset = 0;
		for (dU32 i = 0; i < (dU32)resources.meshes.size(); i++)
			Upload(resources.meshes[i], model.meshes[i], commandList, SubAllocate(staging, offset, GetUploadSize(resources.meshes[i]), Mesh::kUploadAlignment));
	}

	void ResourceManager::Upload(TextureHandle handle, const RHI::TextureData& data, RHI::CommandList& commandList, const RHI::StagingMemory& staging)
	{
		RHI::Texture& texture = m_textures.Get(handle).texture;
		texture.Upload(data, commandList, staging);
		commandList.Transition(texture, RHI::EResourceState::CopyDest, RHI::EResourceState::ShaderResource);
	}

	void ResourceManager::Upload(MeshHandle handle, const MeshData& meshData, RHI::CommandList& commandList, const RHI::StagingMemory& staging)
	{
		Mesh& mesh = m_meshes.Get(handle);
		mesh.Upload(ToMeshDesc(meshData), commandList, staging);
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

	void ResourceManager::FillFrameData(FrameData& frameData) const
	{
		frameData.meshes.assign(m_meshes.GetCapacity(), GPUMeshView{});
		for (dU32 i = 0; i < m_meshes.GetCapacity(); i++)
			if (m_meshes.IsAliveAt(i))
				frameData.meshes[i] = ToMeshView(m_meshes.GetAt(i));

		frameData.materials.assign(m_materials.GetCapacity(), MaterialData{});
		for (dU32 i = 0; i < m_materials.GetCapacity(); i++)
			if (m_materials.IsAliveAt(i))
				frameData.materials[i] = m_materials.GetAt(i).shaderData;

		frameData.sharedSRVHeapCPUAddress = m_srvHeap.GetCPUAddress();
		frameData.sharedSRVHeapCapacity = m_srvHeap.GetCapacity();
	}

	void ResourceManager::Destroy(TextureHandle handle)
	{
		TextureEntry entry = m_textures.Remove(handle);
		entry.texture.Destroy();
		if (entry.srgbSRV.cpuAddress != entry.srv.cpuAddress)
			m_srvHeap.Free(entry.srgbSRV);
		m_srvHeap.Free(entry.srv);
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
