#pragma once

#include "Dune/Platform/RHI/Barrier.h"

namespace Dune::RHI
{
	class Device;
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

		using ResourceHandle = dU32;
		inline constexpr ResourceHandle kInvalidResourceHandle{ (ResourceHandle)-1 };

		enum class EResourceType : dU8
		{
			Texture,
			Buffer
		};

		struct ResourceAccess
		{
			ResourceHandle      handle{ kInvalidResourceHandle };
			dU32                subresource{ RHI::kAllSubresources };
			RHI::EResourceState state{ RHI::EResourceState::Undefined };
		};

		struct ResourceEntry
		{
			RHI::Resource* pPhysicalResource{ nullptr };
			dVector<RHI::EResourceState> subresourceStates;
			RHI::EResourceState resourceState;
			EResourceType type;
			bool isExternal : 1;
		};

		class RenderGraphBuilder
		{
		public:
			static constexpr dU32 kExecuteCapacity = 64;

			void Reset()
			{
				m_reads.clear();
				m_writes.clear();
				m_pExecute = nullptr;
			}

			void Read(ResourceHandle handle, RHI::EResourceState state, dU32 subresource = RHI::kAllSubresources)
			{
				Assert(handle != kInvalidResourceHandle);
				m_reads.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

			void Write(ResourceHandle handle, RHI::EResourceState state, dU32 subresource = RHI::kAllSubresources)
			{
				Assert(handle != kInvalidResourceHandle);
				m_writes.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

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
			void Execute(RenderPassContext& context) const { m_pExecute(m_execute, context); }

			[[nodiscard]] const dVector<ResourceAccess>& GetReads() const { return m_reads; }
			[[nodiscard]] const dVector<ResourceAccess>& GetWrites() const { return m_writes; }
			[[nodiscard]] bool IsEmpty() const { return m_reads.empty() && m_writes.empty(); }

		private:
			dVector<ResourceAccess> m_reads;
			dVector<ResourceAccess> m_writes;
			alignas(16) dU8 m_execute[kExecuteCapacity];
			void (*m_pExecute)(const void*, RenderPassContext&){ nullptr };
		};

		class RenderGraph
		{
		public:
			void Initialize(RHI::Device& device);
			void Destroy();

			[[nodiscard]] ResourceHandle CreateTexture(const RHI::TextureDesc& desc);
			[[nodiscard]] ResourceHandle CreateBuffer(const RHI::BufferDesc& desc);
			[[nodiscard]] ResourceHandle RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] ResourceHandle RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState);
			void SetPhysicalResource(ResourceHandle handle, RHI::Resource* pResource, RHI::EResourceState state);

			[[nodiscard]] RHI::Texture& GetTexture(ResourceHandle handle);
			[[nodiscard]] RHI::Buffer& GetBuffer(ResourceHandle handle);
			[[nodiscard]] inline dU32 GetResourceCount() const { return (dU32)m_resources.size(); }

			void TransitionResource(const ResourceAccess& access, RHI::Barrier& barrier);

		private:
			RHI::Device* m_pDevice{ nullptr };
			dVector<ResourceEntry> m_resources;
		};
	}
}
