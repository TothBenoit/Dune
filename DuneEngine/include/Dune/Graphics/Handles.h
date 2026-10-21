#pragma once

#include "Dune/Core/Handle.h"

namespace Dune::Graphics
{
	class Mesh;
	class Texture;
	struct Material;
	struct MaterialSet;

	using MeshHandle = Handle<Mesh>;
	using TextureHandle = Handle<Texture>;
	using MaterialHandle = Handle<Material>;
	using MaterialSetHandle = Handle<MaterialSet>;
}
