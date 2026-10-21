#pragma once

#include <stdarg.h>
#include <type_traits>

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

namespace Dune::StringUtils
{
	[[nodiscard]] inline const dString printf(const char* fmt, ...)
	{
		char tempStr[4096];

		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(tempStr, 4096, fmt, args);
		va_end(args);

		return dString(tempStr);
	}

	[[nodiscard]] inline const dWString wprintf(const wchar_t* fmt, ...)
	{
		wchar_t tempStr[4096];

		va_list args;
		va_start(args, fmt);
		_vsnwprintf_s(tempStr, 4096, fmt, args);
		va_end(args);

		return dWString(tempStr);
	}

	[[nodiscard]] dWString ToWide(const dString& str);
	[[nodiscard]] dString ToNarrow(const dWString& str);
}

namespace Dune::MemoryUtils
{
	[[nodiscard]] inline dU32 AlignUp(dU32 value, dU32 alignment)
	{
		Assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
		return (value + alignment - 1) & ~(alignment - 1);
	}

	[[nodiscard]] inline dU64 AlignUp(dU64 value, dU64 alignment)
	{
		Assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
		return (value + alignment - 1) & ~(alignment - 1);
	}
}
