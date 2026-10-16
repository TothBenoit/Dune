#include "pch.h"
#include "Dune/Asset/AssetCache.h"
#include "Dune/Core/FileSystem.h"

namespace Dune
{
	void AssetCache::Destroy()
	{
		m_modelLookup.clear();
		m_models.clear();
	}

	ModelHandle AssetCache::FindModel(const char* pPath) const
	{
		auto it = m_modelLookup.find(FileSystem::ResolvePath(pPath));
		return it != m_modelLookup.end() ? it->second : ModelHandle{};
	}

	ModelHandle AssetCache::AddModel(const char* pPath, Model&& model)
	{
		const dString key = FileSystem::ResolvePath(pPath);
		Assert(m_modelLookup.find(key) == m_modelLookup.end());

		const ModelHandle handle = ModelHandle::Make((dU32)m_models.size(), 0);
		model.sourcePath = pPath;
		m_models.push_back(std::move(model));
		m_modelLookup.emplace(key, handle);
		return handle;
	}

	const Model& AssetCache::GetModel(ModelHandle handle) const
	{
		return m_models[handle.GetIndex()];
	}
}
