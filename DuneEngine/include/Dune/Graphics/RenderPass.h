#pragma once

#include "Dune/Graphics/RenderGraph.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Graphics/Material.h"

namespace Dune
{
	namespace Graphics
	{
		class Renderer;
		struct PipelineDesc;
		struct Frame;
		struct FrameData;
		struct ViewData;
		struct PSO;

		using RenderTypeID = const void*;
		template<typename T>
		RenderTypeID GetRenderTypeID() { static dU8 id; return &id; }

		inline std::atomic<dU32> g_renderTypeCount{ 0 };
		template<typename T>
		dU32 GetRenderTypeIndex() { static const dU32 index = g_renderTypeCount.fetch_add(1, std::memory_order_relaxed); return index; }

		enum class ERegistrationOrder : dU8
		{
			Before,
			After
		};

		class RenderBlackboard
		{
		public:
			static constexpr dU32 kInvalidOffset{ (dU32)-1 };

			void Reset()
			{
				m_storage.clear();
				std::fill(m_offsets.begin(), m_offsets.end(), kInvalidOffset);
#ifdef _DEBUG
				std::fill(m_missed.begin(), m_missed.end(), false);
#endif
			}

			template<typename T> void Add(const T& data)
			{
				static_assert(std::is_trivially_copyable_v<T>);
				const dU32 index = GetRenderTypeIndex<T>();
#ifdef _DEBUG
				Assert(index >= m_missed.size() || !m_missed[index]);
#endif
				if (index >= m_offsets.size())
					m_offsets.resize(index + 1, kInvalidOffset);
				if (m_offsets[index] == kInvalidOffset)
				{
					m_offsets[index] = MemoryUtils::AlignUp((dU32)m_storage.size(), (dU32)alignof(T));
					m_storage.resize(m_offsets[index] + sizeof(T));
				}
				memcpy(m_storage.data() + m_offsets[index], &data, sizeof(T));
			}

			template<typename T> [[nodiscard]] const T* TryGet() const
			{
				const dU32 index = GetRenderTypeIndex<T>();
				if (index >= m_offsets.size() || m_offsets[index] == kInvalidOffset)
				{
#ifdef _DEBUG
					if (index >= m_missed.size())
						m_missed.resize(index + 1, false);
					m_missed[index] = true;
#endif
					return nullptr;
				}
				return (const T*)(m_storage.data() + m_offsets[index]);
			}

			template<typename T> [[nodiscard]] const T& Get() const
			{
				const dU32 index = GetRenderTypeIndex<T>();
				Assert(index < m_offsets.size() && m_offsets[index] != kInvalidOffset);

				return *(const T*)(m_storage.data() + m_offsets[index]);
			}

		private:
			dVector<dU8> m_storage;
			dVector<dU32> m_offsets;
#ifdef _DEBUG
			mutable dVector<bool> m_missed;
#endif
		};

		struct RenderPassContext
		{
			const FrameData* pFrameData;
			const ViewData* pView;
			Renderer* pRenderer;
			dU32 passTypeIndex{ 0 };
			RenderBlackboard blackboard;

			[[nodiscard]] PSO* GetPSO(MaterialKey key) const;
			[[nodiscard]] PSO* GetPipeline(dU32 index) const;
			[[nodiscard]] dU32 GetBindlessIndex(RHI::Descriptor srv) const;
			[[nodiscard]] RenderGraph& GetRenderGraph() const;
			[[nodiscard]] RenderGraphBuilder& AddPass();

			template<typename View>
			[[nodiscard]] RHI::Descriptor GetDescriptor(View view) const { return GetRenderGraph().GetDescriptor(view); }
		};

		struct PassPSOs
		{
			PSO* materials[Material::kKeyTableSize]{};
			dVector<PSO*> pipelines;
		};

		struct RenderPass
		{
			void (*pSetup)(RenderPassContext&, void*);
			void (*pShutdown)(Renderer&, void*);
			bool (*pIsPassMaterial)(MaterialKey){ nullptr };
			PipelineDesc (*pGetPipelineDesc)(MaterialKey){ nullptr };
			dVector<PipelineDesc> (*pGetPipelineDescs)(){ nullptr };
			void* pData{ nullptr };
			RenderTypeID typeID{ nullptr };
			dU32 typeIndex{ 0 };

		};

		template<typename T>
		RenderPass MakeRenderPass(Renderer& renderer)
		{
			auto* pPassData = T::Create(renderer);
			using PassData = decltype(pPassData);
			RenderPass pass{};
			pass.pSetup = [](RenderPassContext& context, void* pData) { T::Setup(context, static_cast<PassData>(pData)); };
			pass.pShutdown = [](Renderer& renderer, void* pData) { T::Destroy(renderer, static_cast<PassData>(pData)); };
			if constexpr (requires { T::IsPassMaterial(MaterialKey{}); })
				pass.pIsPassMaterial = &T::IsPassMaterial;
			if constexpr (requires { T::GetPipelineDesc(MaterialKey{}); })
				pass.pGetPipelineDesc = &T::GetPipelineDesc;
			if constexpr (requires { T::GetPipelineDescs(); })
				pass.pGetPipelineDescs = &T::GetPipelineDescs;
			pass.pData = pPassData;
			pass.typeID = GetRenderTypeID<T>();
			pass.typeIndex = GetRenderTypeIndex<T>();
			return pass;
		}
	}
}
