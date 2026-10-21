#pragma once

#include "Dune/Platform/RHI/Resource.h"

namespace Dune::RHI
{
	class Device;

	class Fence : public Resource
	{
	public:
		void Initialize(Device& device, dU64 initialValue);
		void Destroy();

		dU64 GetValue();
		void Wait(dU64 value);
	};
}
