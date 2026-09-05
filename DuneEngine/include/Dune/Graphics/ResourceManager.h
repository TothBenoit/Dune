#pragma once

#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Material.h"
#include "Dune/Resources/Shaders/ShaderInterop.h"

namespace Dune::Graphics
{
	class Device;
	class CommandList;
	class Buffer;

	struct MaterialImportDesc
	{
		dVec3   baseColor{ 1.0f, 1.0f, 1.0f };
		float   metalnessFactor{ 1.0f };
		float   roughnessFactor{ 1.0f };
		dString albedoPath;
		dString normalPath;
		dString roughnessMetalnessPath;
		EAlphaMode alphaMode{ EAlphaMode::Opaque };
		float alphaCutoff{ 0.5f };
		bool doubleSided{ false };
	};

	struct MeshImportDesc
	{
		dVector<Vertex>  vertices;
		dVector<dU32>    indices;
		dVector<SubMesh> subMeshes;
		dU32             materialSlotCount{ 0 };
	};

	struct ModelNode
	{
		dVec3 position;
		dQuat rotation;
		dU32 meshIndex;
		dU32 materialSlotStart;
		dU32 materialSlotCount;
	};

	struct ModelData
	{
		dVector<ModelNode> nodes;
	};

	class ResourceManager
	{
	public:
		void Initialize(Device& device);
		void Destroy();

		[[nodiscard]] dU32 GetTexture(FileSystem::SerializationID<EFileType::Image> id, bool sRGB = false);
		[[nodiscard]] Texture& GetTexture(dU32 index) { return m_textures[index]; }

		[[nodiscard]] const ModelData& GetModel(FileSystem::SerializationID<EFileType::Model> id);
		[[nodiscard]] Mesh& GetMesh(dU32 index) { return m_meshes[index]; }
		[[nodiscard]] const Material& GetMaterial(dU32 id) const { return m_materials[id]; }
		[[nodiscard]] const dVector<Material>& GetMaterials() const { return m_materials; }
		[[nodiscard]] dU32 GetMaterialID(dU32 slot) const { return m_materialIDs[slot]; }

	private:
		void RegisterImageSlot(FileSystem::SerializationID<EFileType::Image> id, dU32 slot);
		[[nodiscard]] dU32 CreateTextureFromPath(CommandList& commandList, dVector<Buffer>& uploadBuffers, const dString& path, bool sRGB);
		[[nodiscard]] dU32 CreateMaterial(const MaterialImportDesc& desc, CommandList& commandList, dVector<Buffer>& uploadBuffers, dVector<dU32>& newTextureSlots);
		[[nodiscard]] dU32 CreateMesh(const MeshImportDesc& desc, CommandList& commandList, dVector<Buffer>& uploadBuffers);
		void ImportModel(const dString& path, ModelData& outModel);
		dU32 ResolveTexture(const dString& path, CommandList& commandList, dVector<Buffer>& uploadBuffers, dVector<dU32>& newTextureSlots, bool sRGB);

	private:
		Device* m_pDevice{ nullptr }; // TODO Cleanup : A ResourceManager is owned by a RenderContext which already own a device. The resource manager should not need this

		dVector<Texture>      m_textures;
		dVector<Mesh>         m_meshes;
		dVector<Material>     m_materials;
		dVector<dU32>         m_materialIDs;

		dVector<dU32>         m_imageLookup;
		dVector<dU32>         m_modelLookup;
		dVector<ModelData>    m_models;
	};
}
