#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"

namespace Dune
{
	// Represent an element to draw
	class GraphicsElement
	{
	public:
		GraphicsElement(const Mesh& mesh, const Shader& shader)
			: m_mesh(mesh), m_shader(shader)
		{}

		inline const Mesh& GetMesh() const { return m_mesh; };
		inline const Shader& GetShader() const { return m_shader; };
	private:
		//TODO: use handle instead of references
		const Mesh& m_mesh;
		const Shader& m_shader;
	};
}

