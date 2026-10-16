#pragma once

#include "Dune/Utilities/Handle.h"
#include "Dune/Graphics/Handles.h"

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
		dString                           sourcePath;
		dVector<Graphics::MaterialHandle> materials;
		dVector<ModelNode>                nodes;
	};

	using ModelHandle = Handle<Model>;

	class AssetCache
	{
	public:
		void Destroy();

		[[nodiscard]] ModelHandle FindModel(const char* pPath) const;
		[[nodiscard]] ModelHandle AddModel(const char* pPath, Model&& model);
		[[nodiscard]] const Model& GetModel(ModelHandle handle) const;

	private:
		dVector<Model>                 m_models;
		dHashMap<dString, ModelHandle> m_modelLookup;
	};
}
