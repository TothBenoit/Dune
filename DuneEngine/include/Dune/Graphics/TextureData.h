#pragma once

#include "Dune/Platform/RHI/Format.h"

namespace Dune::Graphics
{
	struct TextureData
	{
		RHI::EFormat format{ RHI::EFormat::Unknown };
		dU32         width{ 0 };
		dU32         height{ 0 };
		dU32         mipLevels{ 1 };
		dVector<dU8> bytes;
	};
}
