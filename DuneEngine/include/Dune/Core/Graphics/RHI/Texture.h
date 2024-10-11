#pragma once

namespace Dune::Graphics 
{
	struct Device;
	class Texture;

	enum class ETextureUsage
	{
		RTV,
		DSV,
		SRV,
		UAV,
	};

	struct TextureDesc
	{
		const wchar_t* debugName{ nullptr };

		ETextureUsage usage{ ETextureUsage::RTV };
		dU32 dimensions[3]{ 1, 1, 1 };
		dU32 mipLevels{ 1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		float clearValue[4]{ 0.f, 0.f, 0.f, 0.f };
		void* pData{ nullptr };
	};

	[[nodiscard]] Handle<Texture>	CreateTexture(Device* pDevice, const TextureDesc& desc);
	void							ReleaseTexture(Device* pDevice, Handle<Texture> handle);
}
