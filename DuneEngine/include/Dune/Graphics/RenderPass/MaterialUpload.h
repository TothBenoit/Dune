#pragma once

#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct MaterialUploadData
	{
		RHI::Buffer     buffer;
		RHI::Descriptor srv;
		ResourceHandle  handle{ kInvalidResourceHandle };
	};

	class MaterialUpload
	{
	public:
		static MaterialUploadData* Create(Renderer& renderer);
		static void                Setup(RenderGraphBuilder& builder, RenderPassContext& context, MaterialUploadData* pData);
		static void                Execute(RenderPassContext& context, MaterialUploadData* pData);
		static void                Destroy(Renderer& renderer, MaterialUploadData* pData);
	private:
		MaterialUpload() = delete;
	};
}
