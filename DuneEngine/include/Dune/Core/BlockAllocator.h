#pragma once

namespace Dune
{
	class BlockAllocator
	{
	public:
		static constexpr dU32 kInvalidOffset{ (dU32)-1 };

		void Initialize(dU32 count)
		{
			Assert(count != 0);
			m_count = count;
			m_freeSlots.reserve(count);
			for (dU32 i = count; i > 0; i--)
				m_freeSlots.push_back(i - 1);
		}

		void Destroy()
		{
			Assert(m_freeSlots.size() == m_count);
			m_freeSlots.clear();
			m_count = 0;
		}

		[[nodiscard]] dU32 Allocate()
		{
			if (m_freeSlots.empty())
				return kInvalidOffset;
			const dU32 slot = m_freeSlots.back();
			m_freeSlots.pop_back();
			return slot;
		}

		void Free(dU32 slot)
		{
			Assert(slot < m_count && std::find(m_freeSlots.begin(), m_freeSlots.end(), slot) == m_freeSlots.end());
			m_freeSlots.push_back(slot);
		}

	private:
		dU32          m_count{ 0 };
		dVector<dU32> m_freeSlots{};
	};
}
