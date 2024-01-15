#pragma once

#include "Dune/Common/Handle.h"

namespace Dune
{
	class Mesh;
	struct Material
	{
		const float baseColor[3]{ 0.f, 0.f, 0.f };
	};

	// TODO : Do a mesh manager and a material manager
	struct GraphicsComponent
	{
		GraphicsComponent();

		Handle<Mesh> mesh;
		std::shared_ptr<Material> material;
	};
}