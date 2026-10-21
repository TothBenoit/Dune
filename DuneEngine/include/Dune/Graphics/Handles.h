#pragma once

#include "Dune/Core/Handle.h"

namespace Dune::RHI
{
	class Texture;
}

namespace Dune::Graphics
{
	class Mesh;
	struct Material;
	struct MaterialSet;

	using MeshHandle = Handle<Mesh>;
	using TextureHandle = Handle<RHI::Texture>;
	using MaterialHandle = Handle<Material>;
	using MaterialSetHandle = Handle<MaterialSet>;
}
