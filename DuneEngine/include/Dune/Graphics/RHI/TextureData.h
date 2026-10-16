#pragma once

#include "Dune/Graphics/Format.h"

namespace Dune::Graphics
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
