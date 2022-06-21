#include "pch.h"
#include "GraphicsElement.h"

namespace Dune
{
	GraphicsElement::GraphicsElement(const std::shared_ptr<Mesh> mesh, const std::shared_ptr<Material> material, const dMatrix& transform)
		: m_mesh(mesh), m_material(material), m_transform(transform)
	{}
	GraphicsElement::GraphicsElement(const GraphicsElement & other)
		: m_mesh(other.m_mesh), m_material(other.m_material), m_transform(other.m_transform)
	{}
	GraphicsElement& GraphicsElement::operator=(const GraphicsElement & other)
	{
		m_mesh = other.m_mesh;
		m_material = other.m_material;
		m_transform = other.m_transform;
		return *this;
	}
}