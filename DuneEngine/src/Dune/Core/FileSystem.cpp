#include "pch.h"
#include "Dune/Core/FileSystem.h"

namespace Dune
{
	namespace
	{
		struct ResolveTable
		{
			dHashMap<FilePath, dU32> pathToIndex;
			dVector<FilePath> indexToPath;
		};

		struct Root
		{
			dString name;
			dString path;
		};

		static bool g_isInitialized{ false };
		static ResolveTable g_tables[(dU32)EFileType::Count];
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

	dU32 FileSystem::ResolveIndex(EFileType type, const char* path)
	{
		FilePath filePath = ResolvePath(path);
		ResolveTable& table = g_tables[(dU32)type];
		auto it = table.pathToIndex.find(filePath);
		if (it != table.pathToIndex.end())
			return it->second;

		dU32 index = (dU32)table.indexToPath.size();
		table.pathToIndex.emplace(filePath, index);
		table.indexToPath.emplace_back(filePath);
		return index;
	}

	const FilePath& FileSystem::GetIndexPath(EFileType type, dU32 index)
	{
		ResolveTable& table = g_tables[(dU32)type];
		return table.indexToPath[index];
	}
}
