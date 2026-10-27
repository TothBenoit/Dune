#pragma once

#include "Dune/Platform/RHI/Resource.h"
#include "Dune/Platform/RHI/Format.h"
#include "Dune/Platform/RHI/Barrier.h"

namespace Dune::RHI
{
	class Device;
	class CommandList;
	class Buffer;

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
		EResourceState initialState{ EResourceState::Undefined };
	};

	class Texture : public Resource
	{
	public:
		static constexpr dU64 kUploadAlignment{ 512 };

		void Initialize(Device& device, const TextureDesc& desc);
		void Destroy();

		[[nodiscard]] dU64 GetUploadSize() const;
		void Upload(const void* pData, CommandList& commandList, Buffer& stagingBuffer, dU64 stagingOffset);

		[[nodiscard]] const dU32* GetDimensions() const { return m_desc.dimensions; }
		[[nodiscard]] const float* GetClearValue() const { return m_desc.clearValue; }
		[[nodiscard]] EFormat GetFormat() const { return m_desc.format; }
		[[nodiscard]] dU32 GetMipLevels() const { return m_desc.mipLevels; }

		[[nodiscard]] dU64 GetRequiredIntermediateSize(dU32 firstSubresource, dU32 numSubresource) const;
	private:
		friend class Swapchain;

		TextureDesc m_desc;
	};
}
