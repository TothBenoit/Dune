#pragma once

#include "Dune/Graphics/RHI/TextureData.h"

namespace Dune::DDSImporter
{
	[[nodiscard]] bool Import(const char* filePath, Graphics::TextureData& outImage);
}
