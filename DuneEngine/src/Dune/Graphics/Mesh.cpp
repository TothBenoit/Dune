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

	void Mesh::Initialize(Device& device, CommandList& commandList, Buffer& uploadBuffer, const MeshDesc& desc)
	{
		m_subMeshes.assign(desc.pSubMeshes, desc.pSubMeshes + desc.subMeshCount);
		m_materialSlotCount = desc.materialSlotCount;
		m_isIndex32bits = desc.isIndex32bits;
		m_vertexByteStride = desc.vertexByteStride;
		m_vertexCount = desc.vertexCount;
		m_indexCount = desc.indexCount;

		dU32 vertexByteSize = m_vertexCount * m_vertexByteStride;
		dU32 indexByteSize = m_indexCount * (m_isIndex32bits ? sizeof(dU32) : sizeof(dU16));
		dU32 uploadByteSize = vertexByteSize + indexByteSize;

		BufferDesc uploadBufferDesc{ L"UploadBuffer", EBufferUsage::Default, EBufferMemory::CPU, uploadByteSize };
		uploadBuffer.Initialize(device, uploadBufferDesc);

		m_indexBuffer = CreateIndexBuffer(device, indexByteSize);
		UploadBuffer(commandList, m_indexBuffer, uploadBuffer, desc.pIndices, indexByteSize, 0);
		m_vertexBuffer = CreateVertexBuffer(device, vertexByteSize);
		UploadBuffer(commandList, m_vertexBuffer, uploadBuffer, desc.pVertices, vertexByteSize, indexByteSize);
	}

	void Mesh::Destroy()
	{
		m_indexBuffer.Destroy();
		m_vertexBuffer.Destroy();
	}
}
