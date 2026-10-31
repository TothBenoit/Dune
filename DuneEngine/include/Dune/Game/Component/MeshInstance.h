#pragma once

#include "Dune/Core/HandleTypes.h"

namespace Dune
{
	struct MeshInstance
	{
		static constexpr bool kTrackChanges = true;

		MeshHandle        mesh;
		MaterialSetHandle materials;
	};
}
