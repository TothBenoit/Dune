#include "pch.h"
#include "Dune/Graphics/RenderGraph.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Platform/RHI/Texture.h"

namespace Dune::Graphics
{
	void RenderGraph::Initialize(RHI::Device& device)
	{
		m_pDevice = &device;
	}

	void RenderGraph::Destroy()
	{
		for (TextureEntry& entry : m_textures)
		{
			if (entry.pTexture && !entry.isExternal)
			{
				entry.pTexture->Destroy();
				delete entry.pTexture;
			}
		}
		m_textures.clear();

		for (BufferEntry& entry : m_buffers)
		{
			if (entry.pBuffer && !entry.isExternal)
			{
				entry.pBuffer->Destroy();
				delete entry.pBuffer;
			}
		}
		m_buffers.clear();
	}

	RGTexture RenderGraph::CreateTexture(const RHI::TextureDesc& desc)
	{
		RHI::Texture* pTexture = new RHI::Texture();
		pTexture->Initialize(*m_pDevice, desc);

		const dU32 subresourceCount = desc.dimensions[2] * desc.mipLevels;
		RGTexture handle = RegisterTexture(pTexture, desc.initialState, subresourceCount);
		m_textures[handle.index].isExternal = false;
		return handle;
	}

	RGBuffer RenderGraph::CreateBuffer(const RHI::BufferDesc& desc)
	{
		RHI::Buffer* pBuffer = new RHI::Buffer();
		pBuffer->Initialize(*m_pDevice, desc);

		RGBuffer handle = RegisterBuffer(pBuffer, desc.initialState);
		m_buffers[handle.index].isExternal = false;
		return handle;
	}

	RGTexture RenderGraph::RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount)
	{
		Assert(subresourceCount > 0);
		RGTexture handle{ (dU32)m_textures.size() };
		TextureEntry& entry = m_textures.emplace_back();
		entry.pTexture = pTexture;
		entry.subresourceStates.assign(subresourceCount, initialState);
		return handle;
	}

	RGBuffer RenderGraph::RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState)
	{
		RGBuffer handle{ (dU32)m_buffers.size() };
		BufferEntry& entry = m_buffers.emplace_back();
		entry.pBuffer = pBuffer;
		entry.state = initialState;
		return handle;
	}

	void RenderGraph::SetPhysicalResource(RGTexture handle, RHI::Texture* pTexture, RHI::EResourceState state)
	{
		Assert(handle.index < m_textures.size());
		TextureEntry& entry = m_textures[handle.index];
		entry.pTexture = pTexture;
		for (RHI::EResourceState& subresourceState : entry.subresourceStates)
			subresourceState = state;
	}

	void RenderGraph::SetPhysicalResource(RGBuffer handle, RHI::Buffer* pBuffer, RHI::EResourceState state)
	{
		Assert(handle.index < m_buffers.size());
		BufferEntry& entry = m_buffers[handle.index];
		entry.pBuffer = pBuffer;
		entry.state = state;
	}

	RHI::Texture& RenderGraph::GetTexture(RGTexture handle)
	{
		Assert(handle.index < m_textures.size());
		return *m_textures[handle.index].pTexture;
	}

	RHI::Buffer& RenderGraph::GetBuffer(RGBuffer handle)
	{
		Assert(handle.index < m_buffers.size());
		return *m_buffers[handle.index].pBuffer;
	}

	void RenderGraph::TransitionTexture(const TextureAccess& access, RHI::Barrier& barrier)
	{
		TextureEntry& entry = m_textures[access.handle.index];
		if (access.subresource != RHI::kAllSubresources)
		{
			Assert(access.subresource < entry.subresourceStates.size());
			RHI::EResourceState& state = entry.subresourceStates[access.subresource];
			if (state == access.state)
				return;
			barrier.PushTransition(*entry.pTexture, state, access.state, access.subresource);
			state = access.state;
			return;
		}

		bool uniform = true;
		const RHI::EResourceState first = entry.subresourceStates[0];
		for (RHI::EResourceState state : entry.subresourceStates)
		{
			if (state != first)
			{
				uniform = false;
				break;
			}
		}

		if (uniform)
		{
			if (first == access.state)
				return;
			barrier.PushTransition(*entry.pTexture, first, access.state, RHI::kAllSubresources);
			for (RHI::EResourceState& state : entry.subresourceStates)
				state = access.state;
		}
		else
		{
			for (dU32 i = 0; i < (dU32)entry.subresourceStates.size(); i++)
			{
				RHI::EResourceState& state = entry.subresourceStates[i];
				if (state == access.state)
					continue;
				barrier.PushTransition(*entry.pTexture, state, access.state, i);
				state = access.state;
			}
		}
	}

	void RenderGraph::TransitionBuffer(const BufferAccess& access, RHI::Barrier& barrier)
	{
		BufferEntry& entry = m_buffers[access.handle.index];
		if (entry.state == access.state)
			return;
		barrier.PushTransition(*entry.pBuffer, entry.state, access.state, RHI::kAllSubresources);
		entry.state = access.state;
	}
}
