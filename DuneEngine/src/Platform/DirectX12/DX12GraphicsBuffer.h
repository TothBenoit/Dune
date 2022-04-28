#pragma once
#include "Dune/Graphics/GraphicsBuffer.h"
#include <d3d12.h>
#include <wrl.h>

namespace Dune
{
	class DX12GraphicsBuffer : public GraphicsBuffer
	{
	public:
		~DX12GraphicsBuffer() override;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW							m_vertexBufferView;
	};

}

