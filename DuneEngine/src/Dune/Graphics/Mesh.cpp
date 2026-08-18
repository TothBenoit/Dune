#include "pch.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/CommandList.h"

namespace Dune::Graphics
{
	Buffer CreateIndexBuffer(Device& device, dU32 byteSize)
	{
		Assert(byteSize != 0);
		BufferDesc desc{ L"IndexBuffer", EBufferUsage::Default, EBufferMemory::GPU, byteSize };
		Buffer indexBuffer{};
		indexBuffer.Initialize(device, desc);
		return indexBuffer;
	}

	Buffer CreateVertexBuffer(Device& device, dU32 byteSize)
	{
		Assert(byteSize != 0);
		BufferDesc desc{ L"VertexBuffer", EBufferUsage::Default, EBufferMemory::GPU, byteSize };
		Buffer vertexBuffer{};
		vertexBuffer.Initialize(device, desc);
		return vertexBuffer;
	}

	void UploadBuffer(CommandList& commandList, Buffer& buffer, Buffer& uploadBuffer, const void* pData, dU32 byteSize, dU32 byteOffset )
	{
		void* pCpuAddress{ nullptr };
		uploadBuffer.Map(0, 0, &pCpuAddress);
		memcpy((void*)(dU64(pCpuAddress) + byteOffset), pData, byteSize);
		commandList.CopyBufferRegion(buffer, 0, uploadBuffer, byteOffset, byteSize);
		uploadBuffer.Unmap(0, 0);
	}

	void Mesh::Initialize(Device& device, CommandList& commandList, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;
		m_vertexByteStride = vertexByteStride;
		m_isIndex32bits = false;

		dU32 vertexByteSize = vertexCount * vertexByteStride;
		dU32 indexByteSize = indexCount * sizeof(dU16);
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		BufferDesc desc{ L"UploadBuffer", EBufferUsage::Default, EBufferMemory::CPU, uploadByteSize };
		m_uploadBuffer.Initialize(device, desc);

		m_indexBuffer = CreateIndexBuffer(device, indexByteSize);
		UploadBuffer(commandList, m_indexBuffer, m_uploadBuffer, pIndices, indexByteSize, 0);
		m_vertexBuffer = CreateVertexBuffer(device, vertexByteSize);
		UploadBuffer(commandList, m_vertexBuffer, m_uploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Initialize(Device& device, CommandList& commandList, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		m_indexCount = indexCount;
		m_vertexCount = vertexCount;
		m_vertexByteStride = vertexByteStride;
		m_isIndex32bits = true;

		dU32 vertexByteSize = vertexCount * vertexByteStride;
		dU32 indexByteSize = indexCount * sizeof(dU32);
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		BufferDesc desc{ L"UploadBuffer", EBufferUsage::Default, EBufferMemory::CPU, uploadByteSize };
		m_uploadBuffer.Initialize(device, desc);

		m_indexBuffer = CreateIndexBuffer(device, indexByteSize);
		UploadBuffer(commandList, m_indexBuffer, m_uploadBuffer, pIndices, indexByteSize, 0 );
		m_vertexBuffer = CreateVertexBuffer(device, vertexByteSize);
		UploadBuffer(commandList, m_vertexBuffer, m_uploadBuffer, pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Destroy()
	{
		m_uploadBuffer.Destroy();
		m_indexBuffer.Destroy();
		m_vertexBuffer.Destroy();
	}
}
