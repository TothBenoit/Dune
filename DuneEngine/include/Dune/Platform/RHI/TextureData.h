#pragma once

#include "Dune/Platform/RHI/Format.h"

namespace Dune::RHI
{
	struct TextureData
	{
		EFormat      format{ EFormat::Unknown };
		dU32         width{ 0 };
		dU32         height{ 0 };
		dU32         mipLevels{ 1 };
		dVector<dU8> bytes;
	};
}
