#pragma once

#include "Dune/Core/BlockAllocator.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include <mutex>

namespace Dune::Graphics
{
	class DescriptorAllocator
	{
	public:
		void Initialize(RHI::DescriptorHeap& heap, dU32 firstIndex, dU32 capacity)
		{
			Assert(firstIndex + capacity <= heap.GetCapacity());
			m_pHeap = &heap;
			m_firstIndex = firstIndex;
			m_allocator.Initialize(capacity);
		}

		void Destroy()
		{
			m_allocator.Destroy();
			m_pHeap = nullptr;
		}

		[[nodiscard]] RHI::Descriptor Allocate()
		{
			std::lock_guard lock(m_mutex);
			const dU32 offset = m_allocator.Allocate();
			Assert(offset != BlockAllocator::kInvalidOffset);
			return m_pHeap->GetDescriptorAt(m_firstIndex + offset);
		}

		void Free(RHI::Descriptor descriptor)
		{
			Assert(descriptor.IsValid());
			std::lock_guard lock(m_mutex);
			m_allocator.Free(m_pHeap->GetIndex(descriptor) - m_firstIndex);
		}

		void Free(dSpan<const RHI::Descriptor> descriptors)
		{
			std::lock_guard lock(m_mutex);
			for (RHI::Descriptor descriptor : descriptors)
			{
				Assert(descriptor.IsValid());
				m_allocator.Free(m_pHeap->GetIndex(descriptor) - m_firstIndex);
			}
		}

		[[nodiscard]] RHI::DescriptorHeap& GetHeap() const { return *m_pHeap; }

	private:
		RHI::DescriptorHeap* m_pHeap{ nullptr };
		dU32                 m_firstIndex{ 0 };
		BlockAllocator       m_allocator{};
		std::mutex           m_mutex;
	};
}
