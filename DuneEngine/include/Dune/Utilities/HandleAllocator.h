#pragma once

#include "Dune/Utilities/Handle.h"

namespace Dune
{
	template<typename T>
	class HandleAllocator
	{
	public:
		[[nodiscard]] Handle<T> Allocate()
		{
			if (!m_freeIndices.empty())
			{
				const dU32 index = m_freeIndices.back();
				m_freeIndices.pop_back();
				return Handle<T>::Make(index, m_generations[index]);
			}
			m_generations.push_back(0);
			return Handle<T>::Make((dU32)m_generations.size() - 1, 0);
		}

		void Free(Handle<T> handle)
		{
			Assert(IsValid(handle));
			const dU32 index = handle.GetIndex();
			m_generations[index]++;
			m_freeIndices.push_back(index);
		}

		[[nodiscard]] Handle<T> GetHandleAt(dU32 index) const
		{
			return Handle<T>::Make(index, m_generations[index]);
		}

		[[nodiscard]] bool IsValid(Handle<T> handle) const
		{
			const dU32 index = handle.GetIndex();
			return handle.IsValid() && index < m_generations.size() && m_generations[index] == handle.GetGeneration();
		}

	private:
		dVector<dU8>  m_generations;
		dVector<dU32> m_freeIndices;
	};
}
