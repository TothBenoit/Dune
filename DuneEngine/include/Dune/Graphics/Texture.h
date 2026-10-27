#pragma once

#include "Dune/Platform/RHI/Texture.h"
#include "Dune/Graphics/DescriptorAllocator.h"
#include "Dune/Graphics/StagingMemory.h"

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

		void Initialize(RHI::Device& device, BlockDescriptorAllocator& srvHeap, const TextureDesc& desc);
		void Destroy(BlockDescriptorAllocator& srvHeap);

		[[nodiscard]] dU64 GetUploadSize() const;
		void Upload(const void* pData, RHI::CommandList& commandList, const StagingMemory& staging);

		[[nodiscard]] RHI::Descriptor GetView() const { return m_view; }
		[[nodiscard]] RHI::Descriptor ResolveSRGBView(RHI::Device& device, BlockDescriptorAllocator& srvHeap);

	private:
		RHI::Texture    m_texture;
		RHI::Descriptor m_view;
		RHI::Descriptor m_srgbView;
		RHI::EFormat    m_format{ RHI::EFormat::Unknown };
		dU32            m_mipLevels{ 1 };
	};
}
