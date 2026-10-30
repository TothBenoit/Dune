#pragma once

#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"
#include "Dune/Graphics/Pipeline.h"
#include "Dune/Graphics/Material.h"

namespace Dune::Graphics
{
	class Renderer;

	struct ShadowPassData
	{

		dVector<RGTexture> shadowHandles;
		dVector<RGTexture> cubeShadowHandles;
		dVector<RGTexture> activeHandles;
		dVector<RHI::Descriptor> shadowSRVs;
		dVector<RHI::Descriptor> cubeShadowSRVs;
		dVector<ShadowData> shadows;

		RHI::Buffer     shadowsBuffer;
		RHI::Descriptor shadowsSRV;
		RGBuffer        shadowsHandle;
	};

	class Shadow
	{
	public:
		static ShadowPassData* Create(Renderer& renderer);
		static bool            IsPassMaterial(MaterialKey key);
		static PipelineDesc    GetPipelineDesc(MaterialKey key);
		static void            Setup(RenderPassContext& context, ShadowPassData* pData);
		static void            Destroy(Renderer& renderer, ShadowPassData* pData);
	private:
		Shadow() = delete;
	};
}
