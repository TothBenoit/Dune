#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"
#include "Dune/Core/Graphics/Format.h"

namespace Dune::Graphics 
{
	class Device;

	enum class ETextureUsage : dU32
	{
		Invalid = 0,
		RenderTarget = 1 << 0,
		DepthStencil = 1 << 1,
		ShaderResource = 1 << 2,
		UnorderedAccess = 1 << 3,
	};

	inline ETextureUsage operator|(ETextureUsage a, ETextureUsage b)
	{
		return ETextureUsage(dU32(a) | dU32(b));
	}

	inline ETextureUsage operator&(ETextureUsage a, ETextureUsage b)
	{
		return ETextureUsage(dU32(a) & dU32(b));
	}

	struct TextureDesc
	{
		const wchar_t* debugName{ L"Texture" };

		ETextureUsage usage{ ETextureUsage::RenderTarget };
		dU32 dimensions[3]{ 1, 1, 1 };
		dU32 mipLevels{ 1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		float clearValue[4]{ 0.f, 0.f, 0.f, 0.f };
		void* pData{ nullptr };
		dU32 initialState{ 0 };
	};

	class Texture : public Resource
	{
	public:
		void Initialize(Device* pDevice, const TextureDesc& desc);
		void Destroy();

		[[nodiscard]] const dU32* GetDimensions() const { return m_dimensions; }
		[[nodiscard]] const float* GetClearValue() const { return m_clearValue; }
		[[nodiscard]] EFormat GetFormat() const { return m_format; }
	private:		
		ETextureUsage		m_usage;
		EFormat				m_format;
		dU32				m_dimensions[3];
		float				m_clearValue[4];
		dU32				m_state{0};
	};
}
