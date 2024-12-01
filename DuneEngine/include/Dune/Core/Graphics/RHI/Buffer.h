#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Device;

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
		const wchar_t* debugName{ L"Buffer" };

		EBufferUsage	usage{ EBufferUsage::Constant };
		EBufferMemory	memory{ EBufferMemory::CPU };
		const void*		pData{ nullptr };
		dU32			byteSize{ 0 };
		dU32			byteStride{ 0 }; // for structured, vertex and index buffer
		dU32			initialState{ 0 };
	};

	class Buffer : public Resource
	{
	public:
		void Initialize(Device* pDeviceInterface, const BufferDesc& desc);
		void Destroy();

		[[nodiscard]] inline dU32 GetByteSize() const { return m_byteSize; }
		[[nodiscard]] inline dU32 GetByteStride() const { return m_byteStride; }
		[[nodiscard]] inline EBufferUsage GetUsage() const { return m_usage; }
		[[nodiscard]] dU32 GetOffset() const { return m_currentBuffer * m_byteSize; }
		[[nodiscard]] dU32 GetCurrentBufferIndex() const { return m_currentBuffer; }
		[[nodiscard]] dU64 GetGPUAddress();

	private:

		dU32 CycleBuffer();

	private:
		EBufferUsage	m_usage;
		EBufferMemory	m_memory;
		dU32			m_byteSize;
		dU32			m_byteStride;
		dU32			m_currentBuffer;
		dU32			m_state;
	};
}