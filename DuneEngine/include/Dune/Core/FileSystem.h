#pragma once

namespace Dune
{
	enum class EResourceType
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

	[[nodiscard]] dU32 ResolveIndex(EResourceType type, const char* path);
	[[nodiscard]] const dString& GetIndexPath(EResourceType type, dU32 index);

	template<EResourceType type>
	struct SerializationID
	{
		[[nodiscard]] bool IsValid() const { return index != dU32(-1); }
		dU32 index{ dU32(-1) };
	};

	template<EResourceType type>
	[[nodiscard]] SerializationID<type> Resolve(const char* path)
	{
		Assert(IsInitialized());
		return SerializationID<type>{ ResolveIndex(type, path) };
	}

	template<EResourceType type>
	[[nodiscard]] const dString& GetPath(SerializationID<type> id)
	{
		return GetIndexPath(type, id.index);
	}
}
