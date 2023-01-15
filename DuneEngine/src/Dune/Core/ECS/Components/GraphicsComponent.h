#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Mesh;
	struct Material;

	// TODO : Do a mesh manager and a material manager
	struct GraphicsComponent
	{
		GraphicsComponent();

		Handle<Mesh> mesh;
		std::shared_ptr<Material> material;
	};
}