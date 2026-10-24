#pragma once

namespace Dune
{
	using FilePath = dString;

	template<typename T>
	struct PathID
	{
		[[nodiscard]] bool IsValid() const { return index != dU32(-1); }
		[[nodiscard]] bool operator==(const PathID& other) const = default;
		dU32 index{ dU32(-1) };
	};

	class FileSystem
	{
	public:
		static constexpr dU32 kMaxPathTables = 16;

		static void Initialize(const char* engineResourcePath);
		static void Shutdown();
		[[nodiscard]] static bool IsInitialized();

		static void AddRoot(const char* name, const char* path);
		[[nodiscard]] static FilePath ResolvePath(const char* path);

		template<typename T>
		[[nodiscard]] static PathID<T> Resolve(const char* path)
		{
			Assert(IsInitialized());
			return PathID<T>{ ResolveIndex(GetTableIndex<T>(), path) };
		}

		template<typename T>
		[[nodiscard]] static const FilePath& GetPath(PathID<T> id)
		{
			return GetIndexPath(GetTableIndex<T>(), id.index);
		}

	private:
		FileSystem() = delete;

		template<typename T>
		[[nodiscard]] static dU32 GetTableIndex()
		{
			static const dU32 tableIndex = AllocateTable();
			return tableIndex;
		}

		[[nodiscard]] static dU32 AllocateTable();
		[[nodiscard]] static dU32 ResolveIndex(dU32 tableIndex, const char* path);
		[[nodiscard]] static const FilePath& GetIndexPath(dU32 tableIndex, dU32 index);
	};
}

template<typename T>
struct std::hash<Dune::PathID<T>>
{
	[[nodiscard]] size_t operator()(Dune::PathID<T> id) const { return std::hash<Dune::dU32>{}(id.index); }
};
