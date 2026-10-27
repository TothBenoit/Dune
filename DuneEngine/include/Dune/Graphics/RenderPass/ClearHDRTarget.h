#pragma once

#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	class ClearHDRTarget
	{
	public:
		static void* Create(Renderer& renderer);
		static void             Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData);
		static void             Destroy(Renderer& renderer, void* pData);
	private:
		ClearHDRTarget() = delete;
	};
}
