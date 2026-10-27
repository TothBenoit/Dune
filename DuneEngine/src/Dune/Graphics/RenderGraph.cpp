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
		for (ResourceEntry& entry : m_resources)
		{
			if (entry.pPhysicalResource && !entry.isExternal)
			{
				switch (entry.type)
				{
				case EResourceType::Texture:
					static_cast<RHI::Texture*>(entry.pPhysicalResource)->Destroy();
					delete entry.pPhysicalResource;
					break;
				case EResourceType::Buffer:
					static_cast<RHI::Buffer*>(entry.pPhysicalResource)->Destroy();
					delete entry.pPhysicalResource;
					break;
				}
			}
		}
		m_resources.clear();
	}

	ResourceHandle RenderGraph::CreateTexture(const RHI::TextureDesc& desc)
	{
		RHI::Texture* pTexture = new RHI::Texture();
		pTexture->Initialize(*m_pDevice, desc);

		const dU32 subresourceCount = desc.dimensions[2] * desc.mipLevels;
		ResourceHandle handle = RegisterTexture(pTexture, desc.initialState, subresourceCount);
		m_resources[handle].isExternal = false;
		return handle;
	}

	ResourceHandle RenderGraph::CreateBuffer(const RHI::BufferDesc& desc)
	{
		RHI::Buffer* pBuffer = new RHI::Buffer();
		pBuffer->Initialize(*m_pDevice, desc);

		ResourceHandle handle = RegisterBuffer(pBuffer, desc.initialState);
		m_resources[handle].isExternal = false;
		return handle;
	}

	ResourceHandle RenderGraph::RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount)
	{
		Assert(subresourceCount > 0);
		ResourceHandle handle = (ResourceHandle)m_resources.size();
		ResourceEntry entry{};
		entry.pPhysicalResource = pTexture;
		entry.subresourceStates.assign(subresourceCount, initialState);
		entry.isExternal = true;
		entry.type = EResourceType::Texture;
		m_resources.push_back(std::move(entry));
		return handle;
	}

	ResourceHandle RenderGraph::RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState)
	{
		ResourceHandle handle = (ResourceHandle)m_resources.size();
		ResourceEntry entry{};
		entry.pPhysicalResource = pBuffer;
		entry.resourceState = initialState;
		entry.isExternal = true;
		entry.type = EResourceType::Buffer;
		m_resources.push_back(std::move(entry));
		return handle;
	}

	void RenderGraph::SetPhysicalResource(ResourceHandle handle, RHI::Resource* pResource, RHI::EResourceState state)
	{
		Assert(handle < m_resources.size());
		ResourceEntry& entry = m_resources[handle];
		entry.pPhysicalResource = pResource;
		switch (entry.type)
		{
		case EResourceType::Texture:
			for (RHI::EResourceState& subresourceState : entry.subresourceStates)
				subresourceState = state;
			break;
		case EResourceType::Buffer:
			entry.resourceState = state;
			break;
		}
	}

	RHI::Texture& RenderGraph::GetTexture(ResourceHandle handle)
	{
		Assert(handle < m_resources.size());
		Assert(m_resources[handle].type == EResourceType::Texture);
		return *static_cast<RHI::Texture*>(m_resources[handle].pPhysicalResource);
	}

	RHI::Buffer& RenderGraph::GetBuffer(ResourceHandle handle)
	{
		Assert(handle < m_resources.size());
		Assert(m_resources[handle].type == EResourceType::Buffer);
		return *static_cast<RHI::Buffer*>(m_resources[handle].pPhysicalResource);
	}

	void RenderGraph::TransitionResource(const ResourceAccess& access, RHI::Barrier& barrier)
	{
		ResourceEntry& entry = m_resources[access.handle];
		switch (entry.type)
		{
		case EResourceType::Texture:
		{
			if (access.subresource != RHI::kAllSubresources)
			{
				Assert(access.subresource < entry.subresourceStates.size());
				RHI::EResourceState& state = entry.subresourceStates[access.subresource];
				if (state == access.state)
					return;
				barrier.PushTransition(*entry.pPhysicalResource, state, access.state, access.subresource);
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
				barrier.PushTransition(*entry.pPhysicalResource, first, access.state, RHI::kAllSubresources);
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
					barrier.PushTransition(*entry.pPhysicalResource, state, access.state, i);
					state = access.state;
				}
			}
			break;
		}
		case EResourceType::Buffer:
			Assert(access.subresource == RHI::kAllSubresources);
			if (entry.resourceState == access.state)
				return;
			barrier.PushTransition(*entry.pPhysicalResource, entry.resourceState, access.state, RHI::kAllSubresources);
			entry.resourceState = access.state;
			break;
		}
	}
}
