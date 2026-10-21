#pragma once

#include "Dune/Platform/RHI/Buffer.h"

namespace Dune::RHI
{
	struct StagingMemory
	{
		Buffer buffer{};
		dU64   offset{ 0 };
		void*  pCpuAddress{ nullptr };

		[[nodiscard]] bool IsValid() const { return pCpuAddress != nullptr; }
	};
}
