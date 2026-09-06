#pragma once

#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct MaterialUploadData
	{
		Buffer         buffer;
		Descriptor     srv;
		ResourceHandle handle{ kInvalidResourceHandle };
		dU32           srvIndex{ 0 };
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
