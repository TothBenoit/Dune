#include "pch.h"
#include "Mesh.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/EngineCore.h"

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
		GraphicsBufferDesc desc{ EBufferUsage::Default};
		dU32 size{ (dU32)m_vertices.size() * sizeof(Vertex) };
		if (size == 0)
		{
			LOG_ERROR("Vertex buffer is empty");
			return false;
		}

		m_vertexBuffer = EngineCore::GetGraphicsRenderer().CreateBuffer(desc, m_vertices.data(), size);
		return true;
	}

	bool Mesh::UploadIndexBuffer()
	{
		GraphicsBufferDesc desc{ EBufferUsage::Default };
		dU32 size{ (dU32)m_indices.size() * sizeof(dU32) };
		if (size == 0)
		{
			LOG_ERROR("Vertex buffer is empty");
			return false;
		}

		m_indexBuffer = EngineCore::GetGraphicsRenderer().CreateBuffer(desc, m_indices.data(), size);
		return true;
	}

}

