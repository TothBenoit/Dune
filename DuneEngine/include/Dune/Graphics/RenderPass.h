#pragma once

#include "Dune/Graphics/RenderGraph.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	namespace Graphics
	{
		class Renderer;
		struct RenderPass;
		struct PipelineDesc;
		struct Frame;
		struct FrameData;
		struct ViewData;
		struct PSO;

		using RenderTypeID = const void*;
		template<typename T>
		RenderTypeID GetRenderTypeID() { static dU8 id; return &id; }

		enum class ERegistrationOrder : dU8
		{
			Before,
			After
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
					const dU32 offset = MemoryUtils::AlignUp((dU32)m_storage.size(), (dU32)alignof(T));
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
			const ViewData* pView;
			Renderer* pRenderer;
			RenderPass* pPass;
			RenderBlackboard blackboard;

			[[nodiscard]] PSO* GetPSO(MaterialKey key) const;
			[[nodiscard]] PSO* GetPipeline(dU32 index) const;
			[[nodiscard]] dU32 GetBindlessIndex(RHI::Descriptor persistentSRV) const;
			[[nodiscard]] RHI::Descriptor GetGPUDescriptor(const Frame& frame, RHI::Descriptor persistentSRV) const;
		};

		struct RenderPass
		{
			void (*pSetup)(RenderGraphBuilder&, RenderPassContext&, void*);
			void (*pShutdown)(Renderer&, void*);
			bool (*pIsPassMaterial)(MaterialKey){ nullptr };
			PipelineDesc (*pGetPipelineDesc)(MaterialKey){ nullptr };
			dVector<PipelineDesc> (*pGetPipelineDescs)(){ nullptr };
			void* pData{ nullptr };
			RenderTypeID typeID{ nullptr };

			PSO* psos[Material::kKeyTableSize]{};
			dVector<PSO*> pipelines;

			RenderGraphBuilder builder;
		};

		template<typename T>
		RenderPass MakeRenderPass(Renderer& renderer)
		{
			auto* pPassData = T::Create(renderer);
			using PassData = decltype(pPassData);
			RenderPass pass{};
			pass.pSetup = [](RenderGraphBuilder& builder, RenderPassContext& context, void* pData) { T::Setup(builder, context, static_cast<PassData>(pData)); };
			pass.pShutdown = [](Renderer& renderer, void* pData) { T::Destroy(renderer, static_cast<PassData>(pData)); };
			if constexpr (requires { T::IsPassMaterial(MaterialKey{}); })
				pass.pIsPassMaterial = &T::IsPassMaterial;
			if constexpr (requires { T::GetPipelineDesc(MaterialKey{}); })
				pass.pGetPipelineDesc = &T::GetPipelineDesc;
			if constexpr (requires { T::GetPipelineDescs(); })
				pass.pGetPipelineDescs = &T::GetPipelineDescs;
			pass.pData = pPassData;
			pass.typeID = GetRenderTypeID<T>();
			return pass;
		}
	}
}
