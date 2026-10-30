#pragma once

#include "Dune/Platform/RHI/Barrier.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Platform/RHI/Device.h"
#include "Dune/Platform/RHI/Texture.h"
#include "Dune/Core/ChunkedArray.h"

namespace Dune::RHI
{
	class Texture;
	class Buffer;
	struct TextureDesc;
	struct BufferDesc;
}

namespace Dune
{
	namespace Graphics
	{
		struct RenderPassContext;
		class RenderGraph;
		class DescriptorAllocator;

		template<typename T>
		struct ResourceHandle
		{
			static constexpr dU32 kInvalidIndex{ (dU32)-1 };
			dU32 index{ kInvalidIndex };

			[[nodiscard]] bool IsValid() const { return index != kInvalidIndex; }
			[[nodiscard]] bool operator==(const ResourceHandle&) const = default;
		};

		using RGTexture = ResourceHandle<RHI::Texture>;
		using RGBuffer = ResourceHandle<RHI::Buffer>;

		template<typename Desc>
		struct ResourceView
		{
			static constexpr dU32 kInvalidIndex{ (dU32)-1 };
			dU32 index{ kInvalidIndex };

			[[nodiscard]] bool IsValid() const { return index != kInvalidIndex; }
			[[nodiscard]] bool operator==(const ResourceView&) const = default;
		};

		template<typename Desc>
		struct SubresourceView
		{
			RGTexture texture;
			dU32 subresource{ 0 };
		};

		using RGTextureSRV = ResourceView<RHI::SRVTextureDesc>;
		using RGTextureRTV = SubresourceView<RHI::RTVDesc>;
		using RGTextureDSV = SubresourceView<RHI::DSVDesc>;
		using RGBufferSRV = ResourceView<RHI::SRVBufferDesc>;
		using RGBufferUAV = ResourceView<RHI::UAVBufferDesc>;

		template<typename Desc>
		struct CachedView
		{
			Desc desc;
			RHI::Descriptor descriptor;
		};

		template<typename Handle, typename Desc>
		struct ViewRequest
		{
			Handle handle;
			Desc desc;
			bool isDefault{ false };
			RHI::Descriptor descriptor;
		};

		struct TextureAccess
		{
			RGTexture handle;
			dU32 subresource{ RHI::kAllSubresources };
			RHI::EResourceState state{ RHI::EResourceState::Undefined };
		};

		struct BufferAccess
		{
			RGBuffer handle;
			RHI::EResourceState state{ RHI::EResourceState::Undefined };
		};

		struct TextureEntry
		{
			RHI::Texture* pTexture{ nullptr };
			dVector<RHI::EResourceState> subresourceStates;
			bool isExternal{ true };
			RHI::Descriptor defaultSRV;
			dVector<CachedView<RHI::SRVTextureDesc>> srvs;
			dVector<RHI::Descriptor> rtvs;
			dVector<RHI::Descriptor> dsvs;
		};

		struct BufferEntry
		{
			RHI::Buffer* pBuffer{ nullptr };
			RHI::EResourceState state{ RHI::EResourceState::Undefined };
			bool isExternal{ true };
			RHI::Descriptor defaultSRV;
			dVector<CachedView<RHI::SRVBufferDesc>> srvs;
			dVector<CachedView<RHI::UAVBufferDesc>> uavs;
		};

		class RenderGraphBuilder
		{
		public:
			static constexpr dU32 kExecuteCapacity = 64;

			void Reset(RenderGraph& graph, dU32 typeIndex)
			{
				m_textureReads.clear();
				m_textureWrites.clear();
				m_bufferReads.clear();
				m_bufferWrites.clear();
				m_pGraph = &graph;
				m_typeIndex = typeIndex;
				m_pExecute = nullptr;
			}

			void Read(RGTexture handle, RHI::EResourceState state, dU32 subresource = RHI::kAllSubresources)
			{
				Assert(handle.IsValid());
				m_textureReads.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

			void Write(RGTexture handle, RHI::EResourceState state, dU32 subresource = RHI::kAllSubresources)
			{
				Assert(handle.IsValid());
				m_textureWrites.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

			void Read(RGBuffer handle, RHI::EResourceState state)
			{
				Assert(handle.IsValid());
				m_bufferReads.push_back({ .handle = handle, .state = state });
			}

			void Write(RGBuffer handle, RHI::EResourceState state)
			{
				Assert(handle.IsValid());
				m_bufferWrites.push_back({ .handle = handle, .state = state });
			}

			[[nodiscard]] RGTextureSRV ReadSRV(RGTexture handle);
			[[nodiscard]] RGTextureSRV ReadSRV(RGTexture handle, const RHI::SRVTextureDesc& desc);
			[[nodiscard]] RGTextureRTV WriteRTV(RGTexture handle, dU32 mip = 0, dU32 slice = 0);
			[[nodiscard]] RGTextureDSV WriteDSV(RGTexture handle, dU32 mip = 0, dU32 slice = 0);
			[[nodiscard]] RGBufferSRV ReadSRV(RGBuffer handle);
			[[nodiscard]] RGBufferSRV ReadSRV(RGBuffer handle, const RHI::SRVBufferDesc& desc);
			[[nodiscard]] RGBufferUAV WriteUAV(RGBuffer handle, const RHI::UAVBufferDesc& desc);

			template<typename F>
			void OnExecute(F&& execute)
			{
				using Fn = std::decay_t<F>;
				static_assert(std::is_trivially_copyable_v<Fn>);
				static_assert(sizeof(Fn) <= kExecuteCapacity && alignof(Fn) <= 16);
				Assert(!m_pExecute);
				new (m_execute) Fn(std::forward<F>(execute));
				m_pExecute = [](const void* pClosure, RenderPassContext& context) { (*static_cast<const Fn*>(pClosure))(context); };
			}

			[[nodiscard]] bool HasExecute() const { return m_pExecute != nullptr; }
			[[nodiscard]] dU32 GetTypeIndex() const { return m_typeIndex; }
			void Execute(RenderPassContext& context) const { m_pExecute(m_execute, context); }

			[[nodiscard]] const dVector<TextureAccess>& GetTextureReads() const { return m_textureReads; }
			[[nodiscard]] const dVector<TextureAccess>& GetTextureWrites() const { return m_textureWrites; }
			[[nodiscard]] const dVector<BufferAccess>& GetBufferReads() const { return m_bufferReads; }
			[[nodiscard]] const dVector<BufferAccess>& GetBufferWrites() const { return m_bufferWrites; }

		private:
			void PushTextureAccesses(dVector<TextureAccess>& accesses, RGTexture handle, dU32 firstMip, dU32 mipCount, dU32 firstSlice, dU32 sliceCount, RHI::EResourceState state);

		private:
			dVector<TextureAccess> m_textureReads;
			dVector<TextureAccess> m_textureWrites;
			dVector<BufferAccess> m_bufferReads;
			dVector<BufferAccess> m_bufferWrites;
			RenderGraph* m_pGraph{ nullptr };
			dU32 m_typeIndex{ 0 };
			alignas(16) dU8 m_execute[kExecuteCapacity];
			void (*m_pExecute)(const void*, RenderPassContext&){ nullptr };
		};

		class RenderGraph
		{
		public:
			void Initialize(RHI::Device& device, DescriptorAllocator& srvAllocator, DescriptorAllocator& rtvAllocator, DescriptorAllocator& dsvAllocator, dU32 framesInFlight);
			void Destroy();

			void BeginFrame(dU32 frameIndex);
			void ResolveViews();
			void EndFrame();

			[[nodiscard]] RenderGraphBuilder& AddPass(dU32 typeIndex);
			[[nodiscard]] inline dU32 GetPassCount() const { return m_passCount; }
			[[nodiscard]] inline RenderGraphBuilder& GetPass(dU32 index) { Assert(index < m_passCount); return m_passes[index]; }

			[[nodiscard]] RGTexture CreateTexture(const RHI::TextureDesc& desc);
			[[nodiscard]] RGBuffer CreateBuffer(const RHI::BufferDesc& desc);
			[[nodiscard]] RGTexture RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] RGBuffer RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState);
			void SetPhysicalResource(RGTexture handle, RHI::Texture* pTexture, RHI::EResourceState state);
			void SetPhysicalResource(RGBuffer handle, RHI::Buffer* pBuffer, RHI::EResourceState state);
			void ResizeTexture(RGTexture handle, dU32 width, dU32 height);
			void ResizeBuffer(RGBuffer handle, dU32 byteSize);

			[[nodiscard]] RHI::Texture& GetTexture(RGTexture handle);
			[[nodiscard]] RHI::Buffer& GetBuffer(RGBuffer handle);
			[[nodiscard]] inline dU32 GetTextureCount() const { return (dU32)m_textures.size(); }
			[[nodiscard]] inline dU32 GetBufferCount() const { return (dU32)m_buffers.size(); }

			void TransitionTexture(const TextureAccess& access, RHI::Barrier& barrier);
			void TransitionBuffer(const BufferAccess& access, RHI::Barrier& barrier);

			[[nodiscard]] RHI::Descriptor GetDefaultSRV(RGTexture handle) const { return m_textures[handle.index].defaultSRV; }
			[[nodiscard]] RHI::Descriptor GetDescriptor(RGTextureSRV view) const { Assert(m_viewsResolved); return m_textureSRVs[view.index].descriptor; }
			[[nodiscard]] RHI::Descriptor GetDescriptor(RGTextureRTV view) const { return m_textures[view.texture.index].rtvs[view.subresource]; }
			[[nodiscard]] RHI::Descriptor GetDescriptor(RGTextureDSV view) const { return m_textures[view.texture.index].dsvs[view.subresource]; }
			[[nodiscard]] RHI::Descriptor GetDescriptor(RGBufferSRV view) const { Assert(m_viewsResolved); return m_bufferSRVs[view.index].descriptor; }
			[[nodiscard]] RHI::Descriptor GetDescriptor(RGBufferUAV view) const { Assert(m_viewsResolved); return m_bufferUAVs[view.index].descriptor; }

		private:
			friend class RenderGraphBuilder;

			void CreateDefaultViews(TextureEntry& entry);
			void CreateDefaultViews(BufferEntry& entry);
			void ReleaseViews(TextureEntry& entry);
			void ReleaseViews(BufferEntry& entry);

		private:
			RHI::Device* m_pDevice{ nullptr };
			DescriptorAllocator* m_pSRVAllocator{ nullptr };
			DescriptorAllocator* m_pRTVAllocator{ nullptr };
			DescriptorAllocator* m_pDSVAllocator{ nullptr };
			dVector<TextureEntry> m_textures;
			dVector<BufferEntry> m_buffers;
			ChunkedArray<RenderGraphBuilder, 64> m_passes;
			dU32 m_passCount{ 0 };

			dVector<ViewRequest<RGTexture, RHI::SRVTextureDesc>> m_textureSRVs;
			dVector<ViewRequest<RGBuffer, RHI::SRVBufferDesc>> m_bufferSRVs;
			dVector<ViewRequest<RGBuffer, RHI::UAVBufferDesc>> m_bufferUAVs;
			bool m_viewsResolved{ false };

			dVector<dVector<RHI::Descriptor>> m_srvReleases;
			dVector<dVector<RHI::Texture>> m_textureReleases;
			dVector<dVector<RHI::Buffer>> m_bufferReleases;
			dU32 m_frameIndex{ 0 };
		};
	}
}
