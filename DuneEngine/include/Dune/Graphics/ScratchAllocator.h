#pragma once

#include "Dune/Graphics/RHI/StagingMemory.h"

namespace Dune::Graphics
{
	class ScratchAllocator
	{
	public:
		void Initialize(const Buffer& buffer, void* pCpuAddress, dU64 capacity);

		[[nodiscard]] StagingMemory Allocate(dU64 byteSize, dU64 alignment);
		void Reset() { m_offset = 0; }

	private:
		Buffer m_buffer{};
		dU8*   m_pCpuAddress{ nullptr };
		dU64   m_capacity{ 0 };
		dU64   m_offset{ 0 };
	};
}
