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
		};

		struct BufferEntry
		{
			RHI::Buffer* pBuffer{ nullptr };
			RHI::EResourceState state{ RHI::EResourceState::Undefined };
			bool isExternal{ true };
		};

		class RenderGraphBuilder
		{
		public:
			static constexpr dU32 kExecuteCapacity = 64;

			void Reset()
			{
				m_textureReads.clear();
				m_textureWrites.clear();
				m_bufferReads.clear();
				m_bufferWrites.clear();
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

			[[nodiscard]] const dVector<TextureAccess>& GetTextureReads() const { return m_textureReads; }
			[[nodiscard]] const dVector<TextureAccess>& GetTextureWrites() const { return m_textureWrites; }
			[[nodiscard]] const dVector<BufferAccess>& GetBufferReads() const { return m_bufferReads; }
			[[nodiscard]] const dVector<BufferAccess>& GetBufferWrites() const { return m_bufferWrites; }
			[[nodiscard]] bool IsEmpty() const { return m_textureReads.empty() && m_textureWrites.empty() && m_bufferReads.empty() && m_bufferWrites.empty(); }

		private:
			dVector<TextureAccess> m_textureReads;
			dVector<TextureAccess> m_textureWrites;
			dVector<BufferAccess> m_bufferReads;
			dVector<BufferAccess> m_bufferWrites;
			alignas(16) dU8 m_execute[kExecuteCapacity];
			void (*m_pExecute)(const void*, RenderPassContext&){ nullptr };
		};

		class RenderGraph
		{
		public:
			void Initialize(RHI::Device& device);
			void Destroy();

			[[nodiscard]] RGTexture CreateTexture(const RHI::TextureDesc& desc);
			[[nodiscard]] RGBuffer CreateBuffer(const RHI::BufferDesc& desc);
			[[nodiscard]] RGTexture RegisterTexture(RHI::Texture* pTexture, RHI::EResourceState initialState, dU32 subresourceCount = 1);
			[[nodiscard]] RGBuffer RegisterBuffer(RHI::Buffer* pBuffer, RHI::EResourceState initialState);
			void SetPhysicalResource(RGTexture handle, RHI::Texture* pTexture, RHI::EResourceState state);
			void SetPhysicalResource(RGBuffer handle, RHI::Buffer* pBuffer, RHI::EResourceState state);

			[[nodiscard]] RHI::Texture& GetTexture(RGTexture handle);
			[[nodiscard]] RHI::Buffer& GetBuffer(RGBuffer handle);
			[[nodiscard]] inline dU32 GetTextureCount() const { return (dU32)m_textures.size(); }
			[[nodiscard]] inline dU32 GetBufferCount() const { return (dU32)m_buffers.size(); }

			void TransitionTexture(const TextureAccess& access, RHI::Barrier& barrier);
			void TransitionBuffer(const BufferAccess& access, RHI::Barrier& barrier);

		private:
			RHI::Device* m_pDevice{ nullptr };
			dVector<TextureEntry> m_textures;
			dVector<BufferEntry> m_buffers;
		};
	}
}
