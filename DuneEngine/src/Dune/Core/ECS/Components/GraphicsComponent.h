#pragma once

#include "Dune/Graphics/GraphicsElement.h"

namespace Dune
{
	// TODO : Every graphics component has a copy of its mesh, we should use an id to a mesh inside a mesh manager
	struct GraphicsComponent
	{
		Mesh mesh = Mesh(m_defaultCubeIndices, m_defaultCubeVertices);
	private:
		const static dVector<Vertex> m_defaultCubeVertices;
		const static dVector<dU32> m_defaultCubeIndices;
	};
}