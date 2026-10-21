#include "pch.h"
#include "Dune/Graphics/Texture.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/CommandList.h"

namespace Dune::Graphics
{
	void Texture::Initialize(RHI::Device& device, RHI::BlockDescriptorHeap& srvHeap, const TextureDesc& desc)
	{
		m_format = desc.format;
		m_mipLevels = desc.mipLevels;
		m_texture.Initialize(device,
			{
				.usage = RHI::ETextureUsage::ShaderResource,
				.dimensions = { desc.width, desc.height, 1 },
				.mipLevels = desc.mipLevels,
				.format = RHI::ToTypeless(desc.format),
			});

		m_view = srvHeap.Allocate();
		device.CreateSRV(m_view, m_texture, { .mipLevels = m_mipLevels, .format = m_format });
		m_srgbView = {};
	}

	void Texture::Destroy(RHI::BlockDescriptorHeap& srvHeap)
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

	void Texture::Upload(const void* pData, RHI::CommandList& commandList, const RHI::StagingMemory& staging)
	{
		m_texture.Upload(pData, commandList, staging);
		commandList.Transition(m_texture, RHI::EResourceState::CopyDest, RHI::EResourceState::ShaderResource);
	}

	RHI::Descriptor Texture::ResolveSRGBView(RHI::Device& device, RHI::BlockDescriptorHeap& srvHeap)
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
