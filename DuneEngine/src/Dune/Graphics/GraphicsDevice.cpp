#include "pch.h"
#include "GraphicsDevice.h"
#include "Platform/DirectX12/DX12GraphicsDevice.h"

namespace Dune
{
	std::unique_ptr<GraphicsDevice> GraphicsDevice::Create()
	{
#ifdef DUNE_PLATFORM_WINDOWS
		return std::make_unique<DX12GraphicsDevice>(DX12GraphicsDevice());
#else
#error Platform not supported
#endif
	}
}