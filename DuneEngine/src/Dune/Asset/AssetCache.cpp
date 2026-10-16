#include "pch.h"
#include "Dune/Asset/AssetCache.h"

namespace Dune
{
	void AssetCache::Destroy()
	{
		m_materialSets.clear();
		for (Graphics::MeshHandle& primitive : m_primitives)
			primitive = {};
		m_modelLookup.clear();
		m_models.clear();
	}

	ModelHandle AssetCache::FindModel(FileSystem::SerializationID<EFileType::Model> source) const
	{
		auto it = m_modelLookup.find(source.index);
		return it != m_modelLookup.end() ? it->second : ModelHandle{};
	}

	ModelHandle AssetCache::AddModel(Model&& model)
	{
		Assert(model.source.IsValid() && m_modelLookup.find(model.source.index) == m_modelLookup.end());
		const ModelHandle handle = ModelHandle::Make((dU32)m_models.size(), 0);
		m_modelLookup.emplace(model.source.index, handle);
		m_models.push_back(std::move(model));
		return handle;
	}

	const Model& AssetCache::GetModel(ModelHandle handle) const
	{
		return m_models[handle.GetIndex()];
	}

	void AssetCache::SetPrimitive(EPrimitive primitive, Graphics::MeshHandle mesh)
	{
		m_primitives[(dU32)primitive] = mesh;
	}

	Graphics::MeshHandle AssetCache::GetPrimitive(EPrimitive primitive) const
	{
		Assert(m_primitives[(dU32)primitive].IsValid());
		return m_primitives[(dU32)primitive];
	}

	void AssetCache::AddMaterialSet(Graphics::MaterialHandle material, Graphics::MaterialSetHandle materialSet)
	{
		Assert(m_materialSets.find(material.value) == m_materialSets.end());
		m_materialSets.emplace(material.value, materialSet);
	}

	Graphics::MaterialSetHandle AssetCache::FindMaterialSet(Graphics::MaterialHandle material) const
	{
		auto it = m_materialSets.find(material.value);
		return it != m_materialSets.end() ? it->second : Graphics::MaterialSetHandle{};
	}
}
