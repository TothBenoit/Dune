#include "pch.h"
#include "Dune/Core/ECS/Components/GraphicsComponent.h"

namespace Dune
{
	GraphicsComponent::GraphicsComponent()
		: material{ new Material{} }
	{}

}
