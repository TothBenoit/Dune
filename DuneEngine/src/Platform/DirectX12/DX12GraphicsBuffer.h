#pragma once
#include "Dune/Graphics/GraphicsBuffer.h"

namespace Dune
{
	class DX12GraphicsBuffer : public GraphicsBuffer
	{

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;	
		friend class DX12GraphicsRenderer;
	};

}

