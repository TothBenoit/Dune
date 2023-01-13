#pragma once

#include "Dune/Graphics/BufferDesc.h"

namespace Dune
{
	class Buffer
	{
	public:
		~Buffer() = default;

		DISABLE_COPY_AND_MOVE(Buffer);

		inline dU32 GetSize() const { return m_size; };
		inline EBufferUsage GetUsage() const { return m_usage; };
	private:
		Buffer() = default;
	
	private:
		dU32 m_size;
		EBufferUsage m_usage;
		dU8* m_cpuAdress{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;	
		friend class Renderer;
	};
}
