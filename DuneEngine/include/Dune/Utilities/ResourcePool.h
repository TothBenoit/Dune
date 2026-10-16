#pragma once

#include "Dune/Utilities/Handle.h"

namespace Dune
{
	template<typename T, typename Tag = T>
	class ResourcePool
	{
	public:
		using HandleType = Handle<Tag>;

		[[nodiscard]] HandleType Add(T&& item)
		{
			dU32 index;
			if (!m_freeIndices.empty())
			{
				index = m_freeIndices.back();
				m_freeIndices.pop_back();
				m_items[index] = std::move(item);
			}
			else
			{
				index = (dU32)m_items.size();
				m_items.push_back(std::move(item));
				m_generations.push_back(0);
				m_isAlive.push_back(false);
			}
			m_isAlive[index] = true;
			return HandleType::Make(index, m_generations[index]);
		}

		T Remove(HandleType handle)
		{
			Assert(IsValid(handle));
			const dU32 index = handle.GetIndex();
			m_isAlive[index] = false;
			m_generations[index]++;
			m_freeIndices.push_back(index);
			return std::move(m_items[index]);
		}

		[[nodiscard]] bool IsValid(HandleType handle) const
		{
			const dU32 index = handle.GetIndex();
			return handle.IsValid() && index < m_items.size() && m_isAlive[index] && m_generations[index] == handle.GetGeneration();
		}

		[[nodiscard]] T& Get(HandleType handle) { Assert(IsValid(handle)); return m_items[handle.GetIndex()]; }
		[[nodiscard]] const T& Get(HandleType handle) const { Assert(IsValid(handle)); return m_items[handle.GetIndex()]; }

		[[nodiscard]] dU32 GetCapacity() const { return (dU32)m_items.size(); }
		[[nodiscard]] bool IsAliveAt(dU32 index) const { return m_isAlive[index]; }
		[[nodiscard]] const T& GetAt(dU32 index) const { Assert(m_isAlive[index]); return m_items[index]; }
		[[nodiscard]] HandleType GetHandleAt(dU32 index) const { Assert(m_isAlive[index]); return HandleType::Make(index, m_generations[index]); }

	private:
		dVector<T>    m_items;
		dVector<dU8>  m_generations;
		dVector<bool> m_isAlive;
		dVector<dU32> m_freeIndices;
	};
}
