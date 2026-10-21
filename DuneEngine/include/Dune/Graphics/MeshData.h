#pragma once

#include "Dune/Core/RenderTypes.h"

namespace Dune::Graphics
{
	struct MeshData
	{
		dVector<Vertex>  vertices;
		dVector<dU32>    indices;
		dVector<SubMesh> subMeshes;
		dU32             materialSlotCount{ 0 };
	};
}
