#pragma once

#include "Dune/Core/BlockAllocator.h"
#include "Dune/Core/ScratchAllocator.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"

namespace Dune::Graphics
{
	template<typename TAllocator>
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

		template<typename... Args>
		RHI::Descriptor Allocate(Args... args)
		{
			const auto offset = m_allocator.Allocate(args...);
			Assert(offset != TAllocator::kInvalidOffset);
			return m_pHeap->GetDescriptorAt(m_firstIndex + (dU32)offset);
		}

		void Free(RHI::Descriptor descriptor) requires requires(TAllocator allocator) { allocator.Free(0u); }
		{
			Assert(descriptor.IsValid());
			m_allocator.Free(m_pHeap->GetIndex(descriptor) - m_firstIndex);
		}

		void Reset() requires requires(TAllocator allocator) { allocator.Reset(); }
		{
			m_allocator.Reset();
		}

	private:
		RHI::DescriptorHeap* m_pHeap{ nullptr };
		dU32                 m_firstIndex{ 0 };
		TAllocator           m_allocator{};
	};

	using BlockDescriptorAllocator = DescriptorAllocator<BlockAllocator>;
	using ScratchDescriptorAllocator = DescriptorAllocator<ScratchAllocator>;
}
