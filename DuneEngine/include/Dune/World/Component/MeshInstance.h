#pragma once

#include "Dune/Graphics/Handles.h"

namespace Dune
{
	struct MeshInstance
	{
		Graphics::MeshHandle        mesh;
		Graphics::MaterialSetHandle materials;
	};
}
