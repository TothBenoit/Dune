#pragma once

#include "Dune/Platform/RHI/TextureData.h"

namespace Dune::DDSImporter
{
	[[nodiscard]] bool Import(const char* filePath, RHI::TextureData& outImage);
}
