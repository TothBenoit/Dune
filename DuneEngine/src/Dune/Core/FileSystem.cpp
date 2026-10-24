#include "pch.h"
#include "Dune/Core/FileSystem.h"
#include "Dune/Core/ChunkedArray.h"
#include <mutex>

namespace Dune
{
	namespace
	{
		struct ResolveTable
		{
			std::mutex mutex;
			dHashMap<FilePath, dU32> pathToIndex;
			ChunkedArray<FilePath, 256> indexToPath;
			dU32 count{ 0 };
		};

		struct Root
		{
			dString name;
			dString path;
		};

		static bool g_isInitialized{ false };
		static ResolveTable g_tables[FileSystem::kMaxPathTables];
		static std::atomic<dU32> g_tableCount{ 0 };
		static dVector<Root> g_roots;
	}

	void FileSystem::Initialize(const char* engineResourcePath)
	{
		Assert(!g_isInitialized);
		AddRoot("engine", engineResourcePath);
		g_isInitialized = true;
	}

	void FileSystem::Shutdown()
	{
		Assert(g_isInitialized);
		g_isInitialized = false;
	}

	bool FileSystem::IsInitialized()
	{
		return g_isInitialized;
	}

	void FileSystem::AddRoot(const char* name, const char* path)
	{
		Assert(name && path);
		auto itRoot = std::find_if(g_roots.begin(), g_roots.end(), [&](const Root& root) { return root.name.compare(name) == 0; });
		if (itRoot != g_roots.end())
			itRoot->path = path;
		else
			g_roots.push_back({ .name = { name }, .path = { path } });
	}

	FilePath FileSystem::ResolvePath(const char* path)
	{
		Assert(path);

		const char* pSeparator = strstr(path, "://");
		if (!pSeparator)
			return FilePath{ path };

		const dSizeT nameLength = pSeparator - path;
		for (const Root& root : g_roots)
		{
			if (root.name.size() == nameLength && root.name.compare(0, nameLength, path, nameLength) == 0)
				return root.path + (pSeparator + 3);
		}

		Assert(false); // Unknown root
		return FilePath{ path };
	}

	dU32 FileSystem::AllocateTable()
	{
		const dU32 tableIndex = g_tableCount.fetch_add(1, std::memory_order_relaxed);
		Assert(tableIndex < kMaxPathTables);
		return tableIndex;
	}

	dU32 FileSystem::ResolveIndex(dU32 tableIndex, const char* path)
	{
		FilePath filePath = ResolvePath(path);
		ResolveTable& table = g_tables[tableIndex];
		std::lock_guard lock(table.mutex);
		auto it = table.pathToIndex.find(filePath);
		if (it != table.pathToIndex.end())
			return it->second;

		const dU32 index = table.count++;
		table.indexToPath.Resolve(index) = filePath;
		table.pathToIndex.emplace(std::move(filePath), index);
		return index;
	}

	const FilePath& FileSystem::GetIndexPath(dU32 tableIndex, dU32 index)
	{
		return g_tables[tableIndex].indexToPath[index];
	}
}
