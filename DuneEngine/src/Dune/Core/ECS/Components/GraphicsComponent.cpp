#include "pch.h"
#include "GraphicsComponent.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	GraphicsComponent::GraphicsComponent()
		: material{ new Material{} }
	{}

}
