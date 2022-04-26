#pragma once
#ifdef DUNE_PLATFORM_WINDOWS

#include "Dune/Graphics/GraphicsDevice.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace Dune
{
	class DX12GraphicsDevice : public GraphicsDevice
	{
	public:
		DX12GraphicsDevice();
		~DX12GraphicsDevice() {};

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
	};
}

#endif // DUNE_PLATFORM_WINDOWS