#pragma once

#include "Dune/Utilities/Handle.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Graphics/Handles.h"
#include "Dune/Utilities/MeshUtils.h"

namespace Dune
{
	struct ModelNode
	{
		dVec3                       position{ 0.0f, 0.0f, 0.0f };
		dVec4                       rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
		Graphics::MeshHandle        mesh;
		Graphics::MaterialSetHandle materials;
	};

	struct Model
	{
		FileSystem::SerializationID<EFileType::Model> source;
		dVector<Graphics::MaterialHandle> materials;
		dVector<ModelNode>                nodes;
	};

	using ModelHandle = Handle<Model>;

	class AssetCache
	{
	public:
		void Destroy();

		[[nodiscard]] ModelHandle FindModel(FileSystem::SerializationID<EFileType::Model> source) const;
		[[nodiscard]] ModelHandle AddModel(Model&& model);
		[[nodiscard]] const Model& GetModel(ModelHandle handle) const;

		void SetPrimitive(EPrimitive primitive, Graphics::MeshHandle mesh);
		[[nodiscard]] Graphics::MeshHandle GetPrimitive(EPrimitive primitive) const;

		void AddMaterialSet(Graphics::MaterialHandle material, Graphics::MaterialSetHandle materialSet);
		[[nodiscard]] Graphics::MaterialSetHandle FindMaterialSet(Graphics::MaterialHandle material) const;

	private:
		dVector<Model>                              m_models;
		dHashMap<dU32, ModelHandle>                 m_modelLookup;
		Graphics::MeshHandle                        m_primitives[(dU32)EPrimitive::Count];
		dHashMap<dU32, Graphics::MaterialSetHandle> m_materialSets;
	};
}
