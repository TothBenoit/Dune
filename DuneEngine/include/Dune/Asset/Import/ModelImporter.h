#pragma once

#include "Dune/Graphics/ModelData.h"

namespace Dune::ModelImporter
{
	[[nodiscard]] bool Import(const char* filePath, Graphics::ModelData& outModel);
}
