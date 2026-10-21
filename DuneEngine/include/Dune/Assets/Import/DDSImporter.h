#pragma once

#include "Dune/Assets/Image.h"

namespace Dune::DDSImporter
{
	[[nodiscard]] bool Import(const char* filePath, ImageData& outImage);
}
