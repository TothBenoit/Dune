#pragma once

#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/PSOCache.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	struct ShadowData
	{
		PSOHandle shadowPSO[Material::kKeyTableSize];

		dVector<ResourceHandle> shadowHandles;
		dVector<ResourceHandle> cubeShadowHandles;
		dVector<ResourceHandle> activeHandles;

		Buffer         matricesBuffer;
		Descriptor     matricesSRV;
		ResourceHandle matricesHandle{ kInvalidResourceHandle };
	};

	class Shadow
	{
	public:
		static ShadowData* Create(Renderer& renderer);
		static void        Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData);
		static void        Execute(RenderPassContext& context, ShadowData* pData);
		static void        Destroy(Renderer& renderer, ShadowData* pData);
	private:
		Shadow() = delete;
	};
}
