#pragma once

#include "Dune/Platform/RHI/Resource.h"

namespace Dune::RHI
{
	struct PhysicalDeviceDesc
	{
		dString name;
		dU32    vendorID{ 0 };
		dU32    deviceID{ 0 };
		dU64    dedicatedVideoMemory{ 0 };
		dU64    sharedSystemMemory{ 0 };
		bool    isSoftware{ false };
		bool    isSupported{ false };
	};

	class PhysicalDevice : public Resource
	{
	public:
		[[nodiscard]] static dU32 GetPhysicalDeviceCount();
		[[nodiscard]] static PhysicalDeviceDesc GetPhysicalDeviceDesc(dU32 index);

		void Initialize(dU32 index);
		void Destroy();

		[[nodiscard]] PhysicalDeviceDesc GetDesc() const;
	};
}
