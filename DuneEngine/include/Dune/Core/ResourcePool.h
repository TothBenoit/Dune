#pragma once

#include "Dune/Core/Handle.h"
#include "Dune/Core/HandleAllocator.h"

namespace Dune
{
	template<typename T, typename Tag = T>
	class ResourcePool
	{
	public:
		using HandleType = Handle<Tag>;

		void Initialize(const HandleAllocator<Tag>& allocator)
		{
			m_pAllocator = &allocator;
		}

		void Insert(HandleType handle, T&& item)
		{
			Assert(m_pAllocator->IsValid(handle));
			const dU32 index = handle.GetIndex();
			if (index >= m_items.size())
			{
				m_items.resize(index + 1);
				m_isAlive.resize(index + 1, false);
			}
			Assert(!m_isAlive[index]);
			m_items[index] = std::move(item);
			m_isAlive[index] = true;
		}

		T Remove(HandleType handle)
		{
			Assert(IsValid(handle));
			const dU32 index = handle.GetIndex();
			m_isAlive[index] = false;
			return std::move(m_items[index]);
		}

		[[nodiscard]] bool IsValid(HandleType handle) const
		{
			const dU32 index = handle.GetIndex();
			return index < m_items.size() && m_isAlive[index] && m_pAllocator->IsValid(handle);
		}

		[[nodiscard]] T& Get(HandleType handle) { Assert(IsValid(handle)); return m_items[handle.GetIndex()]; }
		[[nodiscard]] const T& Get(HandleType handle) const { Assert(IsValid(handle)); return m_items[handle.GetIndex()]; }

		[[nodiscard]] dU32 GetCapacity() const { return (dU32)m_items.size(); }
		[[nodiscard]] bool IsAliveAt(dU32 index) const { return m_isAlive[index]; }
		[[nodiscard]] const T& GetAt(dU32 index) const { Assert(m_isAlive[index]); return m_items[index]; }
		[[nodiscard]] HandleType GetHandleAt(dU32 index) const { Assert(m_isAlive[index]); return m_pAllocator->GetHandleAt(index); }

	private:
		const HandleAllocator<Tag>* m_pAllocator{ nullptr };
		dVector<T>                  m_items;
		dVector<bool>               m_isAlive;
	};
}
