#pragma once

#include "Dune/Core/Handle.h"
#include "Dune/Core/ChunkedArray.h"

namespace Dune
{
	template<typename T>
	class HandleAllocator
	{
	public:
		static constexpr dU32 kCapacity{ ChunkedArray<dU32>::kCapacity };

		void Destroy()
		{
			m_slots.Destroy();
			m_freeHead.store(MakeHead(kEmptyIndex, 0), std::memory_order_relaxed);
			m_nextFresh.store(0, std::memory_order_relaxed);
		}

		[[nodiscard]] Handle<T> Allocate()
		{
			dU64 head = m_freeHead.load(std::memory_order_acquire);
			while (GetHeadIndex(head) != kEmptyIndex)
			{
				const dU32 index = GetHeadIndex(head);
				const dU32 next = m_slots[index].next.load(std::memory_order_relaxed);
				if (m_freeHead.compare_exchange_weak(head, MakeHead(next, GetHeadTag(head) + 1), std::memory_order_acquire, std::memory_order_acquire))
					return Handle<T>::Make(index, m_slots[index].generation.load(std::memory_order_relaxed));
			}

			const dU32 index = m_nextFresh.fetch_add(1, std::memory_order_relaxed);
			Assert(index < kCapacity);
			Slot& slot = m_slots.Resolve(index);
			return Handle<T>::Make(index, slot.generation.load(std::memory_order_relaxed));
		}

		void Free(Handle<T> handle)
		{
			Assert(IsValid(handle));
			const dU32 index = handle.GetIndex();
			Slot& slot = m_slots[index];
			slot.generation.fetch_add(1, std::memory_order_release);

			dU64 head = m_freeHead.load(std::memory_order_relaxed);
			do
			{
				slot.next.store(GetHeadIndex(head), std::memory_order_relaxed);
			} while (!m_freeHead.compare_exchange_weak(head, MakeHead(index, GetHeadTag(head) + 1), std::memory_order_release, std::memory_order_relaxed));
		}

		[[nodiscard]] Handle<T> GetHandleAt(dU32 index) const
		{
			return Handle<T>::Make(index, m_slots[index].generation.load(std::memory_order_acquire));
		}

		[[nodiscard]] bool IsValid(Handle<T> handle) const
		{
			if (!handle.IsValid())
				return false;
			const Slot* pSlot = m_slots.TryGet(handle.GetIndex());
			return pSlot && pSlot->generation.load(std::memory_order_acquire) == handle.GetGeneration();
		}

	private:
		static constexpr dU32 kEmptyIndex{ dU32(-1) };

		struct Slot
		{
			std::atomic<dU32> generation{ 0 };
			std::atomic<dU32> next{ kEmptyIndex };
		};

		[[nodiscard]] static constexpr dU64 MakeHead(dU32 index, dU32 tag) { return (dU64(tag) << 32) | index; }
		[[nodiscard]] static constexpr dU32 GetHeadIndex(dU64 head) { return dU32(head); }
		[[nodiscard]] static constexpr dU32 GetHeadTag(dU64 head) { return dU32(head >> 32); }

	private:
		ChunkedArray<Slot>  m_slots;
		std::atomic<dU64>   m_freeHead{ MakeHead(kEmptyIndex, 0) };
		std::atomic<dU32>   m_nextFresh{ 0 };
	};
}
