#include "pch.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics.h"

namespace Dune::Graphics
{
	Mesh::Mesh(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
		: m_indexCount{ indexCount }
		, m_vertexCount{ vertexCount }
		, m_pDevice{ pDevice }
	{
		UploadIndexBuffer(pIndices, indexCount, sizeof(dU16));
		UploadVertexBuffer(pVertices, vertexCount, vertexByteStride );
	}

	Mesh::Mesh(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
		: m_indexCount{ indexCount }
		, m_vertexCount{ vertexCount }
		, m_pDevice{ pDevice }
	{
		UploadIndexBuffer(pIndices, indexCount, sizeof(dU32));
		UploadVertexBuffer(pVertices, vertexCount, vertexByteStride);
	}

	Mesh::~Mesh()
	{
		Graphics::ReleaseBuffer(m_pDevice, m_vertexBufferHandle);
		Graphics::ReleaseBuffer(m_pDevice, m_indexBufferHandle);
	}

	void Mesh::UploadIndexBuffer(const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Index, EBufferMemory::GPUStatic, pData, size * byteStride, byteStride };
		m_indexBufferHandle = Graphics::CreateBuffer(m_pDevice, desc);
		Assert(m_indexBufferHandle.IsValid());
	}

	void Mesh::UploadVertexBuffer(const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",EBufferUsage::Vertex, EBufferMemory::GPUStatic, pData, size * byteStride, byteStride};
		m_vertexBufferHandle = Graphics::CreateBuffer(m_pDevice, desc);
		Assert(m_vertexBufferHandle.IsValid());
	}
}

