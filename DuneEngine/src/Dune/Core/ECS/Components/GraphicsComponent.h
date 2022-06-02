#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	// TODO : Do a mesh manager and a material manager
	struct GraphicsComponent
	{
		std::shared_ptr<Mesh> mesh = m_defaultMesh;
		std::shared_ptr<Material> material = std::make_shared<Material>();
	private:
		const static std::shared_ptr<Mesh> m_defaultMesh;
		const static dVector<Vertex> m_defaultCubeVertices;
		const static dVector<dU32> m_defaultCubeIndices;
	};
}