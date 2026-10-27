#pragma once

namespace Dune
{
	class ScratchAllocator
	{
	public:
		static constexpr dU64 kInvalidOffset{ (dU64)-1 };

		void Initialize(dU64 capacity)
		{
			m_capacity = capacity;
			m_offset = 0;
		}

		void Destroy()
		{
			m_capacity = 0;
			m_offset = 0;
		}

		[[nodiscard]] dU64 Allocate(dU64 size, dU64 alignment = 1)
		{
			Assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
			const dU64 offset = (m_offset + alignment - 1) & ~(alignment - 1);
			if (offset + size > m_capacity)
				return kInvalidOffset;

			m_offset = offset + size;
			return offset;
		}

		void Reset() { m_offset = 0; }

	private:
		dU64 m_capacity{ 0 };
		dU64 m_offset{ 0 };
	};
}
