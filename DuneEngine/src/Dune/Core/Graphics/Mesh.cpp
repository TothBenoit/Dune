#include "pch.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics/RHI/Buffer.h"

namespace Dune::Graphics
{

	Buffer* CreateIndexBuffer(Device* pDevice, const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Index, EBufferMemory::GPUStatic, pData, size * byteStride, byteStride };
		Buffer* pIndexBuffer = new Buffer();
		pIndexBuffer->Initialize(pDevice, desc);
		return pIndexBuffer;
	}

	Buffer* CreateVertexBuffer(Device* pDevice, const void* pData, dU32 size, dU32 byteStride)
	{
		Assert(size != 0);
		BufferDesc desc{ L"VertexBuffer",EBufferUsage::Vertex, EBufferMemory::GPUStatic, pData, size * byteStride, byteStride };
		Buffer* pVertexBuffer = new Buffer();
		pVertexBuffer->Initialize(pDevice, desc);
		return pVertexBuffer;
	}

	void Mesh::Initialize(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)		
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;

		m_pIndexBuffer = CreateIndexBuffer(pDevice, pIndices, indexCount, sizeof(dU16));
		m_pVertexBuffer = CreateVertexBuffer(pDevice, pVertices, vertexCount, vertexByteStride );
	}

	void Mesh::Initialize(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;

		m_pIndexBuffer = CreateIndexBuffer(pDevice, pIndices, indexCount, sizeof(dU32));
		m_pVertexBuffer = CreateVertexBuffer(pDevice, pVertices, vertexCount, vertexByteStride);
	}

	void Mesh::Destroy()
	{
		delete m_pIndexBuffer;
		delete m_pVertexBuffer;
	}
}

