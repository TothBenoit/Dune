#pragma once
#include "Dune/Graphics/Buffer.h"

namespace Dune
{
	class DX12Buffer : public Buffer
	{
		~DX12Buffer() = default;
		DISABLE_COPY_AND_MOVE(DX12Buffer)

	private:
		DX12Buffer() = default;
	
	private:
		dU8* m_cpuAdress{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;	
		friend class DX12Renderer;
	};

}

