#include "pch.h"
#include "Mesh.h"
#include "Dune/Core/Logger.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/Buffer.h"

namespace Dune
{
	Mesh::Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices)
		: m_indices(indices), m_vertices(vertices)
	{
		UploadBuffers();
	}

	Mesh::~Mesh()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseBuffer(m_vertexBufferHandle);
		renderer.ReleaseBuffer(m_indexBufferHandle);
	}

	void Mesh::UploadBuffers()
	{
		UploadIndexBuffer();
		UploadVertexBuffer();
	}

	void Mesh::UploadVertexBuffer()
	{
		dU32 size{ (dU32)m_vertices.size() * sizeof(Vertex) };
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer", size, EBufferUsage::Default, m_vertices.data()};
		m_vertexBufferHandle = Renderer::GetInstance().CreateBuffer(desc);
		Assert(m_vertexBufferHandle.IsValid());
	}

	void Mesh::UploadIndexBuffer()
	{
		dU32 size{ (dU32)m_indices.size() * sizeof(dU32) };
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", size, EBufferUsage::Default, m_indices.data() };
		m_indexBufferHandle = Renderer::GetInstance().CreateBuffer(desc);
		Assert(m_indexBufferHandle.IsValid());
	}

}

