#include "pch.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::FileSystem
{
	struct ResolveTable
	{
		dHashMap<dString, dU32> pathToIndex;
		dVector<dString> indexToPath;
	};

	static bool g_isInitialized{ false };
	static ResolveTable g_tables[(dU32)EResourceType::Count];

	void Initialize()
	{
		Assert(!g_isInitialized);
		g_isInitialized = true;
	}

	void Shutdown()
	{
		Assert(g_isInitialized);
		g_isInitialized = false;
	}

	bool IsInitialized()
	{
		return g_isInitialized;
	}

	dU32 ResolveIndex(EResourceType type, const char* path)
	{
		ResolveTable& table = g_tables[(dU32)type];
		auto it = table.pathToIndex.find(path);
		if (it != table.pathToIndex.end())
			return it->second;

		dU32 index = (dU32)table.indexToPath.size();
		table.pathToIndex.emplace(path, index);
		table.indexToPath.emplace_back(path);
		return index;
	}

	const dString& GetIndexPath(EResourceType type, dU32 index)
	{
		ResolveTable& table = g_tables[(dU32)type];
		return table.indexToPath[index];
	}
}
