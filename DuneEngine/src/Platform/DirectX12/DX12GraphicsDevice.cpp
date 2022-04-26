#include "pch.h"

#ifdef DUNE_PLATFORM_WINDOWS

#include "DX12GraphicsDevice.h"

namespace Dune
{

	DX12GraphicsDevice::DX12GraphicsDevice()
	{
		IDXGIFactory4* factory;
		ID3D12Debug1* debugController;

		UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
		// Create a Debug Controller to track errors
		ID3D12Debug* dc;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
		ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(true);

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		dc->Release();
		dc = nullptr;
#endif

		HRESULT result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

		// Declare Handles
		IDXGIAdapter1* adapter;

		// Create Adapter
		for (UINT adapterIndex = 0;
			DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter);
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Don't select the Basic Render Driver adapter.
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			// Check if the adapter supports Direct3D 12, and use that for the rest
			// of the application
			if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0,
				_uuidof(ID3D12Device), nullptr)))
			{
				break;
			}

			// Else we won't use this iteration's adapter, so release it
			adapter->Release();
		}

		// Create Device
		ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_Device)));
	}

}

#endif // DUNE_PLATFORM_WINDOWS