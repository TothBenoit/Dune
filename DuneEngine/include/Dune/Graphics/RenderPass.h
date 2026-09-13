#pragma once

#include "Dune/Graphics/RHI/Barrier.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"

namespace Dune
{
	struct Camera;

	namespace Graphics
	{
		class Renderer;
		struct Frame;
		struct FrameData;

		using ResourceHandle = dU32;
		inline constexpr ResourceHandle kInvalidResourceHandle{ (ResourceHandle)-1 };

		using RenderTypeID = const void*;
		template<typename T>
		RenderTypeID GetRenderTypeID() { static dU8 id; return &id; }

		enum class ERegistrationOrder : dU8
		{
			Before,
			After
		};

		enum class EResourceType : dU8
		{
			Texture,
			Buffer
		};

		struct ResourceAccess
		{
			ResourceHandle handle{ kInvalidResourceHandle };
			dU32           subresource{ kAllSubresources };
			EResourceState state{ EResourceState::Undefined };
		};

		class RenderBlackboard
		{
		public:
			void Reset()
			{
				m_storage.clear();
				m_offsets.clear();
#ifdef _DEBUG
				m_missed.clear();
#endif
			}

			template<typename T> void Add(const T& data)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				RenderTypeID id = GetRenderTypeID<T>();
				Assert(!m_missed.contains(id));
				auto it = m_offsets.find(id);
				if (it == m_offsets.end())
				{
					const dU32 align = (dU32)alignof(T);
					const dU32 offset = ((dU32)m_storage.size() + align - 1) & ~(align - 1);
					m_storage.resize(offset + sizeof(T));
					it = m_offsets.emplace(id, offset).first;
				}
				memcpy(m_storage.data() + it->second, &data, sizeof(T));
			}

			template<typename T> [[nodiscard]] const T* TryGet() const
			{
				RenderTypeID id = GetRenderTypeID<T>();
				auto it = m_offsets.find(id);
				if (it == m_offsets.end())
				{
#ifdef _DEBUG
					m_missed.insert(id);
#endif
					return nullptr;
				}
				return (const T*)(m_storage.data() + it->second);
			}

			template<typename T> [[nodiscard]] const T& Get() const
			{
				RenderTypeID id = GetRenderTypeID<T>();
				auto it = m_offsets.find(id);
				Assert(it != m_offsets.end());

				return *(const T*)(m_storage.data() + it->second);
			}
		
		private:
			dVector<dU8> m_storage;
			dHashMap<RenderTypeID, dU32> m_offsets;
#ifdef _DEBUG
			mutable dHashSet<RenderTypeID> m_missed;
#endif
		};

		struct RenderPassContext
		{
			const FrameData* pFrameData;
			const Camera* pCamera;
			Renderer* pRenderer;
			dVector<dU32> sortedBlendDraw;
			RenderBlackboard blackboard;

			[[nodiscard]] dU32 GetBindlessIndex(Descriptor persistentSRV) const;
			[[nodiscard]] Descriptor GetGPUDescriptor(const Frame& frame, Descriptor persistentSRV) const;
		};

		class RenderGraphBuilder
		{
		public:
			void Reset() 
			{ 
				m_reads.clear(); 
				m_writes.clear(); 
			}

			void Read(ResourceHandle handle, EResourceState state, dU32 subresource = kAllSubresources)
			{
				Assert(handle != kInvalidResourceHandle);
				m_reads.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

			void Write(ResourceHandle handle, EResourceState state, dU32 subresource = kAllSubresources)
			{
				Assert(handle != kInvalidResourceHandle);
				m_writes.push_back({ .handle = handle, .subresource = subresource, .state = state });
			}

			[[nodiscard]] const dVector<ResourceAccess>& GetReads() const { return m_reads; }
			[[nodiscard]] const dVector<ResourceAccess>& GetWrites() const { return m_writes; }
			[[nodiscard]] bool IsEmpty() const { return m_reads.empty() && m_writes.empty(); }

		private:
			dVector<ResourceAccess> m_reads;
			dVector<ResourceAccess> m_writes;
		};

		struct RenderPass
		{
			void (*pSetup)(RenderGraphBuilder&, RenderPassContext&, void*);
			void (*pExecute)(RenderPassContext&, void*);
			void (*pShutdown)(Renderer&, void*);
			void* pData{ nullptr };
			RenderTypeID typeID{ nullptr };

			RenderGraphBuilder builder;
		};
	}
}
