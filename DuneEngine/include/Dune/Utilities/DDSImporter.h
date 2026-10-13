#pragma once

#include "Dune/Graphics/ImageData.h"

namespace Dune::DDSImporter
{
	[[nodiscard]] bool Import(const char* filePath, Graphics::ImageData& outImage);
}
