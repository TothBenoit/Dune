#include "pch.h"
#include "Dune/Graphics/Texture.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/CommandList.h"

namespace Dune::Graphics
{
	void Texture::Initialize(RHI::Device& device, DescriptorAllocator& srvHeap, const TextureDesc& desc)
	{
		m_format = desc.format;
		m_mipLevels = desc.mipLevels;
		m_texture.Initialize(device,
			{
				.usage = RHI::ETextureUsage::ShaderResource,
				.width = desc.width,
				.height = desc.height,
				.mipLevels = desc.mipLevels,
				.format = RHI::ToTypeless(desc.format),
			});

		m_view = srvHeap.Allocate();
		device.CreateSRV(m_view, m_texture, { .mipLevels = m_mipLevels, .format = m_format });
		m_srgbView = {};
	}

	void Texture::Destroy(DescriptorAllocator& srvHeap)
	{
		m_texture.Destroy();
		if (m_srgbView.IsValid() && m_srgbView.cpuAddress != m_view.cpuAddress)
			srvHeap.Free(m_srgbView);
		srvHeap.Free(m_view);
		m_view = {};
		m_srgbView = {};
	}

	dU64 Texture::GetUploadSize() const
	{
		return m_texture.GetUploadSize();
	}

	void Texture::Upload(const void* pData, RHI::CommandList& commandList, const StagingMemory& staging)
	{
		Assert(staging.IsValid());
		RHI::Buffer stagingBuffer{ staging.buffer };
		m_texture.Upload(pData, commandList, stagingBuffer, staging.offset);
		commandList.Transition(m_texture, RHI::EResourceState::CopyDest, RHI::EResourceState::ShaderResource);
	}

	RHI::Descriptor Texture::ResolveSRGBView(RHI::Device& device, DescriptorAllocator& srvHeap)
	{
		if (m_srgbView.IsValid())
			return m_srgbView;

		const RHI::EFormat srgbFormat = RHI::ToSRGB(m_format);
		if (srgbFormat == m_format)
		{
			m_srgbView = m_view;
			return m_srgbView;
		}

		m_srgbView = srvHeap.Allocate();
		device.CreateSRV(m_srgbView, m_texture, { .mipLevels = m_mipLevels, .format = srgbFormat });
		return m_srgbView;
	}
}
