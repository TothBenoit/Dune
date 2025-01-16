#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Device;

	class Fence : public Resource
	{
	public:
		void Initialize(Device* pDevice, dU64 initialValue);
		void Destroy();

		dU64 GetValue();
		void Wait(dU64 value);
	};
}