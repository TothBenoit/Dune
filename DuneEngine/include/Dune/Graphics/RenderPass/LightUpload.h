#pragma once

#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	struct LightUploadData
	{
		RHI::Buffer     buffer;
		RHI::Descriptor srv;
		RGBuffer        handle;
	};

	class LightUpload
	{
	public:
		static LightUploadData* Create(Renderer& renderer);
		static void             Setup(RenderGraphBuilder& builder, RenderPassContext& context, LightUploadData* pData);
		static void             Destroy(Renderer& renderer, LightUploadData* pData);
	private:
		LightUpload() = delete;
	};
}
