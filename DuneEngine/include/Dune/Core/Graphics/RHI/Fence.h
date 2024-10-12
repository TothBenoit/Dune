#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	struct Device;

	class Fence : public Resource
	{
	public:
		Fence(Device* pDevice, dU64 initialValue);
		~Fence();
		DISABLE_COPY_AND_MOVE(Fence);

		dU64 GetValue();
		void Wait(dU64 value);
	};
}