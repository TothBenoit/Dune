#pragma once

#include "Dune/Platform/RHI/Texture.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/StagingMemory.h"

namespace Dune::RHI
{
	class Device;
	class CommandList;
}

namespace Dune::Graphics
{
	struct TextureDesc
	{
		RHI::EFormat format{ RHI::EFormat::Unknown };
		dU32         width{ 0 };
		dU32         height{ 0 };
		dU32         mipLevels{ 1 };
	};

	class Texture
	{
	public:
		static constexpr dU64 kUploadAlignment{ RHI::Texture::kUploadAlignment };

		void Initialize(RHI::Device& device, RHI::BlockDescriptorHeap& srvHeap, const TextureDesc& desc);
		void Destroy(RHI::BlockDescriptorHeap& srvHeap);

		[[nodiscard]] dU64 GetUploadSize() const;
		void Upload(const void* pData, RHI::CommandList& commandList, const RHI::StagingMemory& staging);

		[[nodiscard]] RHI::Descriptor GetView() const { return m_view; }
		[[nodiscard]] RHI::Descriptor ResolveSRGBView(RHI::Device& device, RHI::BlockDescriptorHeap& srvHeap);

	private:
		RHI::Texture    m_texture;
		RHI::Descriptor m_view;
		RHI::Descriptor m_srgbView;
		RHI::EFormat    m_format{ RHI::EFormat::Unknown };
		dU32            m_mipLevels{ 1 };
	};
}
