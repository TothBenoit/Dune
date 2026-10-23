#pragma once

#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct MeshInstance
	{
		MeshHandle        mesh;
		MaterialSetHandle materials;
	};
}
