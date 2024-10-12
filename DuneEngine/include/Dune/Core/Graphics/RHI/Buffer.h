#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	struct Device;
	class Resource;
	struct Descriptor;

	enum class EBufferUsage
	{
		Index,
		Vertex,
		Constant,
		Structured
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
		const void*		pData{ nullptr };
		dU32			byteSize{ 0 };
		dU32			byteStride{ 0 }; // for structured, vertex and index buffer
	};

	class Buffer : public Resource
	{
	public:
		Buffer(Device* pDeviceInterface, const BufferDesc& desc);
		~Buffer();
		DISABLE_COPY_AND_MOVE(Buffer);

		[[nodiscard]] inline dU32 GetByteSize() const { return m_byteSize; }
		[[nodiscard]] inline dU32 GetByteStride() const { return m_byteStride; }
		[[nodiscard]] inline EBufferUsage GetUsage() const { return m_usage; }
		[[nodiscard]] dU32 GetOffset() const { return m_currentBuffer * m_byteSize; }
		[[nodiscard]] dU32 GetCurrentBufferIndex() const { return m_currentBuffer; }
		[[nodiscard]] dU64 GetGPUAddress();
		[[nodiscard]] const Descriptor CreateSRV(); // TODO : desc
		[[nodiscard]] const Descriptor CreateCBV(); // TODO : desc
		void ReleaseDescriptor(const Descriptor& descriptor);		

		void Map(dU32 byteOffset, dU32 byteSize, void** pCpuAdress);
		void Unmap(dU32 byteOffset, dU32 byteSize);
		void UploadData(const void* pData, dU32  byteOffset, dU32 byteSize);

	private:

		dU32 CycleBuffer();

	private:
		const EBufferUsage	m_usage;
		const EBufferMemory m_memory;
		dU32				m_byteSize;
		dU32				m_byteStride;
		dU32				m_currentBuffer;
		Device*				m_pDeviceInterface{ nullptr };

#if _DEBUG
		dU32				m_descriptorAllocated{ 0 };
#endif
	};

	[[nodiscard]] Handle<Buffer>	CreateBuffer(Device* pDevice, const BufferDesc& desc);
	void							ReleaseBuffer(Device* pDevice, Handle<Buffer> handle);
	void							UploadBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize);
	void							MapBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize);
}