#pragma once

#include "Dune/Platform/RHI/Buffer.h"

namespace Dune::Graphics
{
	struct StagingMemory
	{
		RHI::Buffer buffer{};
		dU64   offset{ 0 };
		void*  pCpuAddress{ nullptr };

		[[nodiscard]] bool IsValid() const { return pCpuAddress != nullptr; }
	};
}
