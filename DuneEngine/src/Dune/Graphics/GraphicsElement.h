#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"

namespace Dune
{
	// Represent an element to draw
	class GraphicsElement
	{
	public:
		GraphicsElement(const Mesh& mesh, const Shader& shader, const dMatrix4x4& transform = dMatrix4x4())
			: m_mesh(mesh), m_shader(shader), m_transform(transform)
		{}
		
		GraphicsElement(const GraphicsElement& other)
			: m_mesh(other.m_mesh), m_shader(other.m_shader), m_transform(other.m_transform)
		{}

		GraphicsElement operator=(const GraphicsElement& other)
		{
			return GraphicsElement(other);
		}

		inline const Mesh& GetMesh() const { return m_mesh; };
		inline const Shader& GetShader() const { return m_shader; };
		inline const dMatrix4x4& GetTransform() const { return m_transform; };

	private:
		//TODO: use handle instead of references
		const Mesh& m_mesh;
		const Shader& m_shader;
		
		dMatrix4x4 m_transform;
	};
}

