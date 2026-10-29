#include "pch.h"
#include "Dune/Graphics/RenderGraph.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Platform/RHI/Texture.h"
#include "Dune/Graphics/DescriptorAllocator.h"

namespace Dune::Graphics
{
	static RHI::EFormat ToSRVFormat(RHI::EFormat format)
	{
		switch (format)
		{
		case RHI::EFormat::D32_FLOAT: return RHI::EFormat::R32_FLOAT;
		case RHI::EFormat::D24_UNORM_S8_UINT: return RHI::EFormat::R24_UNORM_X8_TYPELESS;
		case RHI::EFormat::D16_UNORM: return RHI::EFormat::R16_UNORM;
		default: return format;
		}
	}

	template<typename Desc, typename F>
	static RHI::Descriptor ResolveView(dVector<CachedView<Desc>>& cache, const Desc& desc, DescriptorAllocator& allocator, F&& create)
	{
		auto it = std::find_if(cache.begin(), cache.end(), [&](const CachedView<Desc>& view) { return view.desc == desc; });
		if (it != cache.end())
			return it->descriptor;

		const RHI::Descriptor descriptor = allocator.Allocate();
		create(descriptor, desc);
		cache.push_back({ .desc = desc, .descriptor = descriptor });
		return descriptor;
	}

	template<typename Handle, typename Desc>
	static ResourceView<Desc> AddRequest(dVector<ViewRequest<Handle, Desc>>& requests, Handle handle, const Desc& desc, bool isDefault)
	{
		requests.push_back({ .handle = handle, .desc = desc, .isDefault = isDefault });
		return { (dU32)requests.size() - 1 };
	}

	static dU32 GetSubresource(const RHI::Texture& texture, dU32 mip, dU32 slice)
	{
		Assert(mip < texture.GetMipLevels() && slice < texture.GetDimensions()[2]);
		return mip + slice * texture.GetMipLevels();
	}

	static void DestroyTextures(dVector<RHI::Texture>& textures)
	{
		for (RHI::Texture& texture : textures)
			texture.Destroy();
		textures.clear();
	}

	void RenderGraph::Initialize(RHI::Device& device, DescriptorAllocator& srvAllocator, DescriptorAllocator& rtvAllocator, DescriptorAllocator& dsvAllocator, dU32 framesInFlight)
	{
		m_pDevice = &device;
		m_pSRVAllocator = &srvAllocator;
		m_pRTVAllocator = &rtvAllocator;
		m_pDSVAllocator = &dsvAllocator;
		m_srvReleases.resize(framesInFlight);
		m_textureReleases.resize(framesInFlight);
	}

	void RenderGraph::Destroy()
	{
		for (TextureEntry& entry : m_textures)
		{
			ReleaseViews(entry);
			if (entry.pTexture && !entry.isExternal)
			{
				entry.pTexture->Destroy();
				delete entry.pTexture;
			}
		}
		m_textures.clear();

		for (BufferEntry& entry : m_buffers)
		{
			ReleaseViews(entry);
			if (entry.pBuffer && !entry.isExternal)
			{
				entry.pBuffer->Destroy();
				delete entry.pBuffer;
			}
		}
		m_buffers.clear();

		for (dVector<RHI::Descriptor>& releases : m_srvReleases)
			m_pSRVAllocator->Free(releases);
		m_srvReleases.clear();
		for (dVector<RHI::Texture>& releases : m_textureReleases)
			DestroyTextures(releases);
		m_textureReleases.clear();
	}

	void RenderGraph::BeginFrame(dU32 frameIndex)
	{
		Assert(!m_viewsResolved);
		m_frameIndex = frameIndex;
		m_pSRVAllocator->Free(m_srvReleases[frameIndex]);
		m_srvReleases[frameIndex].clear();
		DestroyTextures(m_textureReleases[frameIndex]);
	}

	void RenderGraph::ResolveViews()
	{
		Assert(!m_viewsResolved);
		for (ViewRequest<RGTexture, RHI::SRVTextureDesc>& request : m_textureSRVs)
		{
			TextureEntry& entry = m_textures[request.handle.index];
			request.descriptor = request.isDefault ? entry.defaultSRV
				: ResolveView(entry.srvs, request.desc, *m_pSRVAllocator, [&](RHI::Descriptor descriptor, const RHI::SRVTextureDesc& desc) { m_pDevice->CreateSRV(descriptor, *entry.pTexture, desc); });
			Assert(request.descriptor.IsValid());
		}
		for (ViewRequest<RGBuffer, RHI::SRVBufferDesc>& request : m_bufferSRVs)
		{
			BufferEntry& entry = m_buffers[request.handle.index];
			request.descriptor = ResolveView(entry.srvs, request.desc, *m_pSRVAllocator, [&](RHI::Descriptor descriptor, const RHI::SRVBufferDesc& desc) { m_pDevice->CreateSRV(descriptor, *entry.pBuffer, desc); });
		}
		for (ViewRequest<RGBuffer, RHI::UAVBufferDesc>& request : m_bufferUAVs)
		{
			BufferEntry& entry = m_buffers[request.handle.index];
			request.descriptor = ResolveView(entry.uavs, request.desc, *m_pSRVAllocator, [&](RHI::Descriptor descriptor, const RHI::UAVBufferDesc& desc) { m_pDevice->CreateUAV(descriptor, *entry.pBuffer, desc); });
		}
		m_viewsResolved = true;
	}

	void RenderGraph::EndFrame()
	{
		m_textureSRVs.clear();
		m_bufferSRVs.clear();
		m_bufferUAVs.clear();
		m_viewsResolved = false;
	}

	void RenderGraph::CreateDefaultViews(TextureEntry& entry)
	{
		RHI::Texture& texture = *entry.pTexture;
		const RHI::TextureDesc& desc = texture.GetDesc();
		const dU32 arraySize = desc.dimensions[2];
		if ((desc.usage & RHI::ETextureUsage::ShaderResource) != RHI::ETextureUsage::Invalid)
		{
			entry.defaultSRV = m_pSRVAllocator->Allocate();
			m_pDevice->CreateSRV(entry.defaultSRV, texture, { .mipLevels = desc.mipLevels, .arraySize = arraySize, .format = ToSRVFormat(desc.format), .dimension = arraySize > 1 ? RHI::ESRVDimension::Texture2DArray : RHI::ESRVDimension::Texture2D });
		}
		const bool isRenderTarget = (desc.usage & RHI::ETextureUsage::RenderTarget) != RHI::ETextureUsage::Invalid;
		const bool isDepthStencil = (desc.usage & RHI::ETextureUsage::DepthStencil) != RHI::ETextureUsage::Invalid;
		for (dU32 slice = 0; slice < arraySize; slice++)
		{
			for (dU32 mip = 0; mip < desc.mipLevels; mip++)
			{
				if (isRenderTarget)
				{
					const RHI::Descriptor rtv = entry.rtvs.emplace_back(m_pRTVAllocator->Allocate());
					m_pDevice->CreateRTV(rtv, texture, { .mipSlice = mip, .firstArraySlice = slice });
				}
				if (isDepthStencil)
				{
					const RHI::Descriptor dsv = entry.dsvs.emplace_back(m_pDSVAllocator->Allocate());
					m_pDevice->CreateDSV(dsv, texture, { .mipSlice = mip, .firstArraySlice = slice });
				}
			}
		}
	}

	void RenderGraph::ReleaseViews(TextureEntry& entry)
	{
		dVector<RHI::Descriptor>& releases = m_srvReleases[m_frameIndex];
		if (entry.defaultSRV.IsValid())
			releases.push_back(entry.defaultSRV);
		for (const CachedView<RHI::SRVTextureDesc>& view : entry.srvs)
			releases.push_back(view.descriptor);
		m_pRTVAllocator->Free(entry.rtvs);
		m_pDSVAllocator->Free(entry.dsvs);
		entry.defaultSRV = {};
		entry.srvs.clear();
		entry.rtvs.clear();
		entry.dsvs.clear();
	}

	void RenderGraph::ReleaseViews(BufferEntry& entry)
	{
		dVector<RHI::Descriptor>& releases = m_srvReleases[m_frameIndex];
		for (const CachedView<RHI::SRVBufferDesc>& view : entry.srvs)
			releases.push_back(view.descriptor);
		for (const CachedView<RHI::UAVBufferDesc>& view : entry.uavs)
			releases.push_back(view.descriptor);
		entry.srvs.clear();
		entry.uavs.clear();
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
		CreateDefaultViews(entry);
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
		Assert(handle.index < m_textures.size() && !m_viewsResolved);
		TextureEntry& entry = m_textures[handle.index];
		ReleaseViews(entry);
		entry.pTexture = pTexture;
		for (RHI::EResourceState& subresourceState : entry.subresourceStates)
			subresourceState = state;
		CreateDefaultViews(entry);
	}

	void RenderGraph::SetPhysicalResource(RGBuffer handle, RHI::Buffer* pBuffer, RHI::EResourceState state)
	{
		Assert(handle.index < m_buffers.size() && !m_viewsResolved);
		BufferEntry& entry = m_buffers[handle.index];
		ReleaseViews(entry);
		entry.pBuffer = pBuffer;
		entry.state = state;
	}

	void RenderGraph::ResizeTexture(RGTexture handle, dU32 width, dU32 height)
	{
		TextureEntry& entry = m_textures[handle.index];
		Assert(!entry.isExternal);
		RHI::TextureDesc desc = entry.pTexture->GetDesc();
		desc.dimensions[0] = width;
		desc.dimensions[1] = height;
		m_textureReleases[m_frameIndex].push_back(*entry.pTexture);
		entry.pTexture->Initialize(*m_pDevice, desc);
		SetPhysicalResource(handle, entry.pTexture, desc.initialState);
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

	void RenderGraphBuilder::PushTextureAccesses(dVector<TextureAccess>& accesses, RGTexture handle, dU32 firstMip, dU32 mipCount, dU32 firstSlice, dU32 sliceCount, RHI::EResourceState state)
	{
		Assert(handle.IsValid());
		const RHI::Texture& texture = m_pGraph->GetTexture(handle);
		const dU32 mipLevels = texture.GetMipLevels();
		const dU32 arraySize = texture.GetDimensions()[2];
		Assert(firstMip < mipLevels && firstSlice < arraySize);
		mipCount = std::min(mipCount, mipLevels - firstMip);
		sliceCount = std::min(sliceCount, arraySize - firstSlice);
		if (mipCount == mipLevels && sliceCount == arraySize)
		{
			accesses.push_back({ .handle = handle, .state = state });
			return;
		}

		for (dU32 slice = firstSlice; slice < firstSlice + sliceCount; slice++)
			for (dU32 mip = firstMip; mip < firstMip + mipCount; mip++)
				accesses.push_back({ .handle = handle, .subresource = mip + slice * mipLevels, .state = state });
	}

	RGTextureSRV RenderGraphBuilder::ReadSRV(RGTexture handle)
	{
		Read(handle, RHI::EResourceState::ShaderResource);
		return AddRequest(m_pGraph->m_textureSRVs, handle, RHI::SRVTextureDesc{}, true);
	}

	RGTextureSRV RenderGraphBuilder::ReadSRV(RGTexture handle, const RHI::SRVTextureDesc& desc)
	{
		PushTextureAccesses(m_textureReads, handle, desc.mipStart, desc.mipLevels, desc.firstArraySlice, desc.arraySize, RHI::EResourceState::ShaderResource);
		return AddRequest(m_pGraph->m_textureSRVs, handle, desc, false);
	}

	RGTextureRTV RenderGraphBuilder::WriteRTV(RGTexture handle, dU32 mip, dU32 slice)
	{
		const dU32 subresource = GetSubresource(m_pGraph->GetTexture(handle), mip, slice);
		Write(handle, RHI::EResourceState::RenderTarget, subresource);
		return { .texture = handle, .subresource = subresource };
	}

	RGTextureDSV RenderGraphBuilder::WriteDSV(RGTexture handle, dU32 mip, dU32 slice)
	{
		const dU32 subresource = GetSubresource(m_pGraph->GetTexture(handle), mip, slice);
		Write(handle, RHI::EResourceState::DepthStencil, subresource);
		return { .texture = handle, .subresource = subresource };
	}

	RGBufferSRV RenderGraphBuilder::ReadSRV(RGBuffer handle, const RHI::SRVBufferDesc& desc)
	{
		Read(handle, RHI::EResourceState::ShaderResource);
		return AddRequest(m_pGraph->m_bufferSRVs, handle, desc, false);
	}

	RGBufferUAV RenderGraphBuilder::WriteUAV(RGBuffer handle, const RHI::UAVBufferDesc& desc)
	{
		Write(handle, RHI::EResourceState::UAV);
		return AddRequest(m_pGraph->m_bufferUAVs, handle, desc, false);
	}
}
