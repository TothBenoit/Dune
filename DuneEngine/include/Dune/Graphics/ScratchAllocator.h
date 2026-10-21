#pragma once

#include "Dune/Platform/RHI/StagingMemory.h"

namespace Dune::Graphics
{
	class ScratchAllocator
	{
	public:
		void Initialize(const RHI::Buffer& buffer, void* pCpuAddress, dU64 capacity);

		[[nodiscard]] RHI::StagingMemory Allocate(dU64 byteSize, dU64 alignment);
		void Reset() { m_offset = 0; }

	private:
		RHI::Buffer m_buffer{};
		dU8*        m_pCpuAddress{ nullptr };
		dU64        m_capacity{ 0 };
		dU64        m_offset{ 0 };
	};
}
