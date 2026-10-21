#pragma once

namespace Dune
{
	template<typename T>
	struct Handle
	{
		static constexpr dU32 kIndexBits{ 24 };
		static constexpr dU32 kIndexMask{ (1u << kIndexBits) - 1 };
		static constexpr dU32 kInvalidValue{ dU32(-1) };

		[[nodiscard]] static Handle Make(dU32 index, dU8 generation)
		{
			Assert(index < kIndexMask);
			return Handle{ (dU32(generation) << kIndexBits) | index };
		}

		[[nodiscard]] dU32 GetIndex() const { return value & kIndexMask; }
		[[nodiscard]] dU8 GetGeneration() const { return dU8(value >> kIndexBits); }
		[[nodiscard]] bool IsValid() const { return value != kInvalidValue; }
		[[nodiscard]] bool operator==(const Handle& other) const = default;

		dU32 value{ kInvalidValue };
	};
}

template<typename T>
struct std::hash<Dune::Handle<T>>
{
	[[nodiscard]] size_t operator()(Dune::Handle<T> handle) const { return std::hash<Dune::dU32>{}(handle.value); }
};
