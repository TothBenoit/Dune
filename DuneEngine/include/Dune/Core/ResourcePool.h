#pragma once

#include "Dune/Core/Handle.h"
#include "Dune/Core/HandleAllocator.h"
#include "Dune/Core/ChunkedArray.h"

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

		void Destroy()
		{
			m_entries.Destroy();
			m_capacity.store(0, std::memory_order_relaxed);
			m_pAllocator = nullptr;
		}

		void Insert(HandleType handle, T&& item)
		{
			Assert(m_pAllocator->IsValid(handle));
			const dU32 index = handle.GetIndex();
			Entry& entry = m_entries.Resolve(index);
			Assert(!entry.isAlive);
			entry.item = std::move(item);
			entry.isAlive = true;

			dU32 capacity = m_capacity.load(std::memory_order_relaxed);
			while (capacity <= index && !m_capacity.compare_exchange_weak(capacity, index + 1, std::memory_order_release, std::memory_order_relaxed)) {}
		}

		T Remove(HandleType handle)
		{
			Assert(IsValid(handle));
			Entry& entry = m_entries[handle.GetIndex()];
			entry.isAlive = false;
			return std::move(entry.item);
		}

		[[nodiscard]] bool IsValid(HandleType handle) const
		{
			const Entry* pEntry = m_entries.TryGet(handle.GetIndex());
			return pEntry && pEntry->isAlive && m_pAllocator->IsValid(handle);
		}

		[[nodiscard]] T& Get(HandleType handle) { Assert(IsValid(handle)); return m_entries[handle.GetIndex()].item; }
		[[nodiscard]] const T& Get(HandleType handle) const { Assert(IsValid(handle)); return m_entries[handle.GetIndex()].item; }

		[[nodiscard]] dU32 GetCapacity() const { return m_capacity.load(std::memory_order_acquire); }
		[[nodiscard]] bool IsAliveAt(dU32 index) const { const Entry* pEntry = m_entries.TryGet(index); return pEntry && pEntry->isAlive; }
		[[nodiscard]] const T& GetAt(dU32 index) const { Assert(IsAliveAt(index)); return m_entries[index].item; }
		[[nodiscard]] HandleType GetHandleAt(dU32 index) const { Assert(IsAliveAt(index)); return m_pAllocator->GetHandleAt(index); }

	private:
		struct Entry
		{
			T    item{};
			bool isAlive{ false };
		};

		const HandleAllocator<Tag>* m_pAllocator{ nullptr };
		ChunkedArray<Entry>         m_entries;
		std::atomic<dU32>           m_capacity{ 0 };
	};
}
