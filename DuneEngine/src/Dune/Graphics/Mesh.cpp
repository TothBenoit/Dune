#include "pch.h"
#include "Mesh.h"
#include "Dune/Core/Logger.h"
#include "Dune/Graphics/GraphicsCore.h"

namespace Dune
{
	Mesh::Mesh(const dVector<dU32>& indices, const dVector<Vertex>& vertices)
		: m_indices(indices), m_vertices(vertices)
	{}

	void Mesh::UploadBuffers()
	{
		if (!UploadIndexBuffer())
		{
			return;
		}

		if (!UploadVertexBuffer())
		{
			m_indexBuffer.release();
			return;
		}

		m_isUploaded = true;
	}

	bool Mesh::UploadVertexBuffer()
	{
		GraphicsBufferDesc desc;
		desc.size = (dU32)m_vertices.size() * sizeof(Vertex);
		if (desc.size == 0)
		{
			LOG_ERROR("Vertex buffer is empty");
			return false;
		}

		m_vertexBuffer = GraphicsCore::GetGraphicsRenderer().CreateBuffer(m_vertices.data(), desc);
		return true;
	}

	bool Mesh::UploadIndexBuffer()
	{
		GraphicsBufferDesc desc;
		desc.size = (dU32)m_indices.size() * sizeof(dU32);
		if (desc.size == 0)
		{
			LOG_ERROR("Index buffer is empty");
			return false;
		}

		m_indexBuffer = GraphicsCore::GetGraphicsRenderer().CreateBuffer(m_indices.data(), desc);
		return true;
	}

}

