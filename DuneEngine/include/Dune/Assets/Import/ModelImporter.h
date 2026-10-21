#pragma once

#include "Dune/Assets/Model.h"

namespace Dune::ModelImporter
{
	[[nodiscard]] bool Import(const char* filePath, ModelData& outModel);
}
