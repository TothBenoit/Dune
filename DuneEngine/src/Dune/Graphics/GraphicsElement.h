#pragma once

#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/Shader.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	// Represent an element to draw
	class GraphicsElement
	{
	public:
		GraphicsElement(const std::shared_ptr<Mesh> mesh, const std::shared_ptr<Material> material, const dMatrix& transform);
		
		GraphicsElement(const GraphicsElement& other);

		GraphicsElement& operator=(const GraphicsElement& other);

		inline const Mesh* GetMesh() const { return m_mesh.get(); };
		inline const Material* GetMaterial() const { return m_material.get(); }
		inline const dMatrix& GetTransform() const { return m_transform; };

	private:
		//TODO: use handle instead of references
		std::shared_ptr<Mesh> m_mesh;
		std::shared_ptr<Material> m_material;
		dMatrix m_transform;
	};
}

