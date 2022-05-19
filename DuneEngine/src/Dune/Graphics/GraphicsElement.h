#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"

namespace Dune
{
	// Represent an element to draw
	class GraphicsElement
	{
	public:
		GraphicsElement(const Mesh* mesh, const dMatrix& transform = dMatrix())
			: m_mesh(mesh), m_transform(transform)
		{}
		
		GraphicsElement(const GraphicsElement& other)
			: m_mesh(other.m_mesh), m_transform(other.m_transform)
		{}

		GraphicsElement operator=(const GraphicsElement& other)
		{
			return GraphicsElement(other);
		}

		inline const Mesh* GetMesh() const { return m_mesh; };
		inline const dMatrix& GetTransform() const { return m_transform; };

	private:
		//TODO: use handle instead of references
		const Mesh* m_mesh;
		
		dMatrix m_transform;
	};
}

