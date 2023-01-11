#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	// TODO : Do a mesh manager and a material manager
	struct GraphicsComponent
	{
		GraphicsComponent();

		std::shared_ptr<Mesh> mesh;
		std::shared_ptr<Material> material;
	};
}