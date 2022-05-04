#include "pch.h"
#include "Mesh.h"

Dune::Mesh::Mesh(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices)
	: m_indices(indices), m_vertices(vertices)
{}

void Dune::Mesh::UploadBuffer()
{
	GraphicsBufferDesc desc = GraphicsBufferDesc();
	desc.size = (uint32_t)m_indices.size() * sizeof(uint32_t);
	m_indexBuffer = GraphicsCore::GetGraphicsRenderer().CreateBuffer(m_indices.data(), desc);

	desc.size = (uint32_t)m_vertices.size() * sizeof(Vertex);
	m_vertexBuffer = GraphicsCore::GetGraphicsRenderer().CreateBuffer(m_vertices.data(), desc);

	m_isUploaded = true;
}
