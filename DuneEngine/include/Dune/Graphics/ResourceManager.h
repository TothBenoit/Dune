#pragma once

#include "Dune/Graphics/DescriptorAllocator.h"
#include "Dune/Core/HandleTypes.h"
#include "Dune/Platform/RHI/StagingMemory.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Graphics/Texture.h"
#include "Dune/Graphics/TextureData.h"
#include "Dune/Assets/Model.h"
#include "Dune/Core/ResourcePool.h"
#include "Dune/Core/HandleAllocator.h"

namespace Dune::RHI
{
	class Device;
	class CommandList;
}

namespace Dune::Graphics
{
	struct GPUResourceTable;

	struct MaterialSet
	{
		dVector<MaterialHandle> materials;
	};

	struct ModelResources
	{
		dVector<MaterialHandle>    materials;
		dVector<MeshHandle>        meshes;
		dVector<MaterialSetHandle> nodeMaterialSets;
	};

	struct ResourceCache
	{
		[[nodiscard]] ModelResources AllocateModel(const ModelData& model);
		[[nodiscard]] TextureHandle GetTexture(ImageHandle image) const;
		[[nodiscard]] const ModelResources& GetModelResources(ModelHandle model) const;

		HandleAllocator<Texture>              textures;
		HandleAllocator<Material>             materials;
		HandleAllocator<MaterialSet>          materialSets;
		HandleAllocator<Mesh>                 meshes;
		dHashMap<ImageHandle, TextureHandle>  imageTextures;
		dHashMap<ModelHandle, ModelResources> modelResources;
	};

	class ResourceManager
	{
	public:
		static constexpr dU32 kSharedSRVCapacity = 512;

		void Initialize(RHI::Device& device, const ResourceCache& resourceCache);
		void Destroy();

		void CreateTexture(TextureHandle handle, const TextureData& data);
		void CreateTexture(TextureHandle handle, const ImageData& image);
		void CreateMaterial(MaterialHandle handle, const MaterialDesc& desc);
		void CreateMaterialSet(MaterialSetHandle handle, dSpan<MaterialHandle> materials);
		void CreateMesh(MeshHandle handle, const MeshData& meshData);
		void CreateModel(const ModelResources& handles, const ModelData& model, dSpan<TextureHandle> images);

		[[nodiscard]] dU64 GetUploadSize(TextureHandle handle) const;
		[[nodiscard]] dU64 GetUploadSize(MeshHandle handle) const;
		[[nodiscard]] dU64 GetUploadSize(const ModelResources& resources) const;
		void Upload(TextureHandle handle, const TextureData& data, RHI::CommandList& commandList, const RHI::StagingMemory& staging);
		void Upload(TextureHandle handle, const ImageData& image, RHI::CommandList& commandList, const RHI::StagingMemory& staging);
		void Upload(MeshHandle handle, const MeshData& meshData, RHI::CommandList& commandList, const RHI::StagingMemory& staging);
		void Upload(const ModelResources& resources, const ModelData& model, RHI::CommandList& commandList, const RHI::StagingMemory& staging);

		void Destroy(TextureHandle handle);
		void Destroy(MaterialHandle handle);
		void Destroy(MaterialSetHandle handle);
		void Destroy(MeshHandle handle);

		void FillResourceTable(GPUResourceTable& table) const;

		[[nodiscard]] const Mesh& GetMesh(MeshHandle handle) const { return m_meshes.Get(handle); }
		[[nodiscard]] const Material& GetMaterial(MaterialHandle handle) const { return m_materials.Get(handle); }
		[[nodiscard]] const MaterialSet& GetMaterialSet(MaterialSetHandle handle) const { return m_materialSets.Get(handle); }
		[[nodiscard]] const ResourcePool<Mesh>& GetMeshes() const { return m_meshes; }
		[[nodiscard]] const ResourcePool<Material>& GetMaterials() const { return m_materials; }
		[[nodiscard]] const RHI::DescriptorHeap& GetSRVHeap() const { return m_srvHeap; }

	private:
		void CreateMesh(MeshHandle handle, const MeshDesc& desc);
		void Upload(MeshHandle handle, const MeshDesc& desc, RHI::CommandList& commandList, const RHI::StagingMemory& staging);
		[[nodiscard]] dU32 GetViewIndex(TextureHandle handle) const;
		[[nodiscard]] dU32 ResolveSRGBViewIndex(TextureHandle handle);

	private:
		RHI::Device* m_pDevice{ nullptr }; // TODO Cleanup : A ResourceManager is owned by a RenderContext which already own a device. The resource manager should not need this

		ResourcePool<Texture>               m_textures;
		ResourcePool<Material>              m_materials;
		ResourcePool<MaterialSet>           m_materialSets;
		ResourcePool<Mesh>                  m_meshes;
		RHI::DescriptorHeap                 m_srvHeap;
		BlockDescriptorAllocator            m_srvAllocator;
	};
}
