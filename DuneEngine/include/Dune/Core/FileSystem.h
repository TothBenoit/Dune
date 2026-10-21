#pragma once

namespace Dune
{
	enum class EFileType
	{
		Image,
		Model,
		Shader,
		Count
	};

	using FilePath = dString;

	class FileSystem
	{
	public:
		static void Initialize(const char* engineResourcePath);
		static void Shutdown();
		[[nodiscard]] static bool IsInitialized();

		static void AddRoot(const char* name, const char* path);
		[[nodiscard]] static FilePath ResolvePath(const char* path);

		template<EFileType type>
		struct SerializationID
		{
			[[nodiscard]] bool IsValid() const { return index != dU32(-1); }
			[[nodiscard]] bool operator==(const SerializationID& other) const = default;
			dU32 index{ dU32(-1) };
		};

		template<EFileType type>
		[[nodiscard]] static SerializationID<type> Resolve(const char* path)
		{
			Assert(IsInitialized());
			return SerializationID<type>{ ResolveIndex(type, path) };
		}

		template<EFileType type>
		[[nodiscard]] static const FilePath& GetPath(SerializationID<type> id)
		{
			return GetIndexPath(type, id.index);
		}
	private:
		FileSystem() = delete;
		[[nodiscard]] static dU32 ResolveIndex(EFileType type, const char* path);
		[[nodiscard]] static const FilePath& GetIndexPath(EFileType type, dU32 index);
	};
}

template<Dune::EFileType type>
struct std::hash<Dune::FileSystem::SerializationID<type>>
{
	[[nodiscard]] size_t operator()(Dune::FileSystem::SerializationID<type> id) const { return std::hash<Dune::dU32>{}(id.index); }
};
