#pragma once

#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shaders/ShaderInterop.h"

namespace Dune::Graphics
{
	class Device;
	class CommandList;
	class Buffer;

	struct ModelNode
	{
		dVec3 position;
		dQuat rotation;
		dU32 meshIndex;
		dU32 materialIndex;
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

		[[nodiscard]] dU32 GetTexture(FileSystem::SerializationID<EResourceType::Image> id, bool sRGB = false);
		[[nodiscard]] Texture& GetTexture(dU32 index) { return m_textures[index]; }

		[[nodiscard]] const ModelData& GetModel(FileSystem::SerializationID<EResourceType::Model> id);
		[[nodiscard]] Mesh& GetMesh(dU32 index) { return m_meshes[index]; }
		[[nodiscard]] MaterialData& GetMaterial(dU32 index) { return m_materials[index]; }

	private:
		void RegisterImageSlot(FileSystem::SerializationID<EResourceType::Image> id, dU32 slot);
		[[nodiscard]] dU32 CreateTextureFromPath(CommandList& commandList, dVector<Buffer>& uploadBuffers, const dString& path, bool sRGB);
		void ImportModel(const dString& path, ModelData& outModel);

	private:
		Device* m_pDevice{ nullptr }; // TODO Cleanup : A ResourceManager is owned by a RenderContext which already own a device. The resource manager should not need this

		dVector<Texture>      m_textures;
		dVector<Mesh>         m_meshes;
		dVector<MaterialData> m_materials;

		dVector<dU32>         m_imageLookup;
		dVector<dU32>         m_modelLookup;
		dVector<ModelData>    m_models;
	};
}
