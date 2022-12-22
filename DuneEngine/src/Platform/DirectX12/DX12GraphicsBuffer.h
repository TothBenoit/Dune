#pragma once
#include "Dune/Graphics/GraphicsBuffer.h"

namespace Dune
{
	class DX12GraphicsBuffer : public GraphicsBuffer
	{
		~DX12GraphicsBuffer() = default;

		DX12GraphicsBuffer(const DX12GraphicsBuffer& other) = delete;
		DX12GraphicsBuffer& operator=(const DX12GraphicsBuffer& other) = delete;

		DX12GraphicsBuffer(DX12GraphicsBuffer&& other)
			: m_cpuAdress {other.m_cpuAdress}
			, m_buffer {std::move(other.m_buffer)}
		{}

		DX12GraphicsBuffer& operator=(DX12GraphicsBuffer&& other)
		{
			m_cpuAdress = other.m_cpuAdress;
			m_buffer = std::move(other.m_buffer);

			return *this;
		}


	private:
		DX12GraphicsBuffer() = default;
	
	private:
		dU8* m_cpuAdress{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;	
		friend class DX12GraphicsRenderer;
	};

}

