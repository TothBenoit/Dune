#pragma once
#include "Dune/Graphics/Buffer.h"

namespace Dune
{
	class DX12Buffer : public Buffer
	{
		~DX12Buffer() = default;

		DX12Buffer(const DX12Buffer& other) = delete;
		DX12Buffer& operator=(const DX12Buffer& other) = delete;

		DX12Buffer(DX12Buffer&& other)
			: m_cpuAdress {other.m_cpuAdress}
			, m_buffer {std::move(other.m_buffer)}
		{}

		DX12Buffer& operator=(DX12Buffer&& other)
		{
			m_cpuAdress = other.m_cpuAdress;
			m_buffer = std::move(other.m_buffer);

			return *this;
		}


	private:
		DX12Buffer() = default;
	
	private:
		dU8* m_cpuAdress{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;	
		friend class DX12Renderer;
	};

}

