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
		BufferDesc desc{ EBufferUsage::Default};
		dU32 size{ (dU32)m_vertices.size() * sizeof(Vertex) };
		Assert(size != 0);
		m_vertexBufferHandle = Renderer::GetInstance().CreateBuffer(desc, m_vertices.data(), size);
		Assert(m_vertexBufferHandle.IsValid());
	}

	void Mesh::UploadIndexBuffer()
	{
		BufferDesc desc{ EBufferUsage::Default };
		dU32 size{ (dU32)m_indices.size() * sizeof(dU32) };
		Assert(size != 0);
		m_indexBufferHandle = Renderer::GetInstance().CreateBuffer(desc, m_indices.data(), size);
		Assert(m_indexBufferHandle.IsValid());
	}

}

