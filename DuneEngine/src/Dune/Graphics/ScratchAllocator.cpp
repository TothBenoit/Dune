#include "pch.h"
#include "Dune/Graphics/ScratchAllocator.h"

namespace Dune::Graphics
{
	void ScratchAllocator::Initialize(const RHI::Buffer& buffer, void* pCpuAddress, dU64 capacity)
	{
		m_buffer = buffer;
		m_pCpuAddress = (dU8*)pCpuAddress;
		m_capacity = capacity;
		m_offset = 0;
	}

	RHI::StagingMemory ScratchAllocator::Allocate(dU64 byteSize, dU64 alignment)
	{
		Assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
		const dU64 offset = (m_offset + alignment - 1) & ~(alignment - 1);
		if (offset + byteSize > m_capacity)
			return {};

		m_offset = offset + byteSize;
		return { .buffer = m_buffer, .offset = offset, .pCpuAddress = m_pCpuAddress + offset };
	}
}
