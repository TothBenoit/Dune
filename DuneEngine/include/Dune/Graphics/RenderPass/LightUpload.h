#pragma once

#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct LightUploadData
	{
		Buffer         buffer;
		Descriptor     srv;
		ResourceHandle handle{ kInvalidResourceHandle };
		dU32           srvIndex{ 0 };
		dU32           lightCount{ 0 };
	};

	class LightUpload
	{
	public:
		static LightUploadData* Create(Renderer& renderer);
		static void             Setup(RenderGraphBuilder& builder, RenderPassContext& context, LightUploadData* pData);
		static void             Execute(RenderPassContext& context, LightUploadData* pData);
		static void             Destroy(Renderer& renderer, LightUploadData* pData);
	private:
		LightUpload() = delete;
	};
}
