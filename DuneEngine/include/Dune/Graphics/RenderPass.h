#pragma once

#include "Dune/Graphics/RHI/Barrier.h"

namespace Dune
{
	struct Camera;

	namespace Graphics
	{
		class Renderer;
		struct FrameData;

		using ResourceHandle = dU32;
		inline constexpr ResourceHandle kInvalidResourceHandle{ (ResourceHandle)-1 };

		using RenderPassTypeID = const void*;
		template<typename T>
		RenderPassTypeID GetRenderPassTypeID() { static dU8 id; return &id; }

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

		struct RenderPassContext
		{
			Renderer* pRenderer;
			Camera* pCamera;
			FrameData* pFrameData;
			Barrier* pBarrier;
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
			RenderPassTypeID typeID{ nullptr };

			RenderGraphBuilder builder;
		};
	}
}
