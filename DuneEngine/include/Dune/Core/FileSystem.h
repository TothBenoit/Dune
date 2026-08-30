#pragma once

namespace Dune
{
	enum class EFileType
	{
		Image,
		Model,
		Count
	};
}

namespace Dune::FileSystem
{
	void Initialize();
	void Shutdown();
	[[nodiscard]] bool IsInitialized();

	[[nodiscard]] dU32 ResolveIndex(EFileType type, const char* path);
	[[nodiscard]] const dString& GetIndexPath(EFileType type, dU32 index);

	template<EFileType type>
	struct SerializationID
	{
		[[nodiscard]] bool IsValid() const { return index != dU32(-1); }
		dU32 index{ dU32(-1) };
	};

	template<EFileType type>
	[[nodiscard]] SerializationID<type> Resolve(const char* path)
	{
		Assert(IsInitialized());
		return SerializationID<type>{ ResolveIndex(type, path) };
	}

	template<EFileType type>
	[[nodiscard]] const dString& GetPath(SerializationID<type> id)
	{
		return GetIndexPath(type, id.index);
	}
}
