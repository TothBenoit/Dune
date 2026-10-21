#pragma once

namespace Dune
{
	template<typename T>
	struct Handle
	{
		static constexpr dU64 kInvalidValue{ dU64(-1) };

		[[nodiscard]] static Handle Make(dU32 index, dU32 generation)
		{
			Assert(index != dU32(-1));
			return Handle{ (dU64(generation) << 32) | index };
		}

		[[nodiscard]] dU32 GetIndex() const { return dU32(value); }
		[[nodiscard]] dU32 GetGeneration() const { return dU32(value >> 32); }
		[[nodiscard]] bool IsValid() const { return value != kInvalidValue; }
		[[nodiscard]] bool operator==(const Handle& other) const = default;

		dU64 value{ kInvalidValue };
	};
}

template<typename T>
struct std::hash<Dune::Handle<T>>
{
	[[nodiscard]] size_t operator()(Dune::Handle<T> handle) const { return std::hash<Dune::dU64>{}(handle.value); }
};
