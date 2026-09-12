#pragma once

namespace Dune::HashUtils
{
	// FNV-1a
	inline constexpr dU64 kHashOffsetBasis{ 14695981039346656037ull };
	inline constexpr dU64 kHashPrime{ 1099511628211ull };

	inline void HashBytes(dU64& hash, const void* pData, dSizeT byteSize)
	{
		const dU8* pBytes = (const dU8*)pData;
		for (dSizeT i = 0; i < byteSize; i++)
		{
			hash ^= pBytes[i];
			hash *= kHashPrime;
		}
	}

	template<typename T>
	inline void HashValue(dU64& hash, const T& value)
	{
		static_assert(std::is_scalar_v<T>);
		HashBytes(hash, &value, sizeof(T));
	}
}
