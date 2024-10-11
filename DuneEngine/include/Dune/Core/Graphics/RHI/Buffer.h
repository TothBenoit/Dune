#pragma once

namespace Dune::Graphics
{
	struct Device;
	class Buffer;

	enum class EBufferUsage
	{
		Vertex,
		Index,
		Constant,
		Structured,
	};

	enum class EBufferMemory
	{
		CPU,
		GPU,
		GPUStatic
	};

	struct BufferDesc
	{
		const wchar_t* debugName = nullptr;

		EBufferUsage	usage{ EBufferUsage::Constant };
		EBufferMemory	memory{ EBufferMemory::CPU };
		const void* pData{ nullptr };
		dU32			byteSize{ 0 };
		dU32			byteStride{ 0 }; // for structured, vertex and index buffer
	};

	[[nodiscard]] Handle<Buffer>	CreateBuffer(Device* pDevice, const BufferDesc& desc);
	void							ReleaseBuffer(Device* pDevice, Handle<Buffer> handle);
	void							UploadBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize);
	void							MapBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize);
}