#pragma once

namespace Dune
{
	enum class ETextureUsage
	{
		RTV,
		DSV,
	};


	struct TextureDesc
	{
		const wchar_t* debugName = nullptr;

		ETextureUsage usage = ETextureUsage::RTV;
		dU32 dimensions[3] = { 1, 1, 1 };
		DXGI_FORMAT	format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		D3D12_CLEAR_VALUE clearValue = { .Format = format, .Color{ 0.f, 0.f, 0.f, 0.f } };
	};
}