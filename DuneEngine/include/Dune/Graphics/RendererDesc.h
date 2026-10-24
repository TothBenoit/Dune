#pragma once

#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	struct RenderPassDesc
	{
		RenderTypeID typeID{ nullptr };
		RenderPass (*pMake)(Renderer&){ nullptr };
	};

	struct RendererDesc
	{
		dVector<RenderPassDesc> passes;

		template<typename T>
		void Add()
		{
			Assert(Find(GetRenderTypeID<T>()) == passes.end());
			passes.push_back(MakeRenderPassDesc<T>());
		}

		template<typename T, typename Neighbor>
		void Insert(ERegistrationOrder order)
		{
			Assert(Find(GetRenderTypeID<T>()) == passes.end());
			auto it = Find(GetRenderTypeID<Neighbor>());
			Assert(it != passes.end());
			if (order == ERegistrationOrder::After)
				it = std::next(it);
			passes.insert(it, MakeRenderPassDesc<T>());
		}

		template<typename Old, typename New>
		void Replace()
		{
			Assert((std::is_same_v<Old, New> || Find(GetRenderTypeID<New>()) == passes.end()));
			auto it = Find(GetRenderTypeID<Old>());
			Assert(it != passes.end());
			*it = MakeRenderPassDesc<New>();
		}

		template<typename T>
		void Remove()
		{
			auto it = Find(GetRenderTypeID<T>());
			Assert(it != passes.end());
			passes.erase(it);
		}

		[[nodiscard]] dVector<RenderPassDesc>::iterator Find(RenderTypeID typeID)
		{
			auto it = passes.begin();
			while (it != passes.end() && it->typeID != typeID)
				++it;
			return it;
		}

	private:
		template<typename T>
		static RenderPassDesc MakeRenderPassDesc()
		{
			return { .typeID = GetRenderTypeID<T>(), .pMake = &MakeRenderPass<T> };
		}
	};
}
