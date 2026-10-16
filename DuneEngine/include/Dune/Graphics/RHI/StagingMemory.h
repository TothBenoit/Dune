#pragma once

#include "Dune/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{
	struct StagingMemory
	{
		Buffer buffer{};
		dU64   offset{ 0 };
		void*  pCpuAddress{ nullptr };

		[[nodiscard]] bool IsValid() const { return pCpuAddress != nullptr; }
	};
}
