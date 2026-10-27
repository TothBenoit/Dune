#pragma once

#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Pipeline.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	struct ShadowData
	{

		dVector<ResourceHandle> shadowHandles;
		dVector<ResourceHandle> cubeShadowHandles;
		dVector<ResourceHandle> activeHandles;

		RHI::Buffer     matricesBuffer;
		RHI::Descriptor matricesSRV;
		ResourceHandle  matricesHandle{ kInvalidResourceHandle };
	};

	class Shadow
	{
	public:
		static ShadowData*  Create(Renderer& renderer);
		static bool         IsPassMaterial(MaterialKey key);
		static PipelineDesc GetPipelineDesc(MaterialKey key);
		static void         Setup(RenderGraphBuilder& builder, RenderPassContext& context, ShadowData* pData);
		static void         Destroy(Renderer& renderer, ShadowData* pData);
	private:
		Shadow() = delete;
	};
}
