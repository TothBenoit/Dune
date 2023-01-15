#pragma once

namespace Dune
{
	class Mesh;
	struct Material;

	// TODO : Do a mesh manager and a material manager
	struct GraphicsComponent
	{
		GraphicsComponent();

		std::shared_ptr<Mesh> mesh;
		std::shared_ptr<Material> material;
	};
}