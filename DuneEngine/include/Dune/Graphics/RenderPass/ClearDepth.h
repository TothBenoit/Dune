#pragma once

#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RenderPass.h"

namespace Dune::Graphics
{
	class Renderer;

	class ClearDepth
	{
	public:
		static void*            Create(Renderer& renderer);
		static void             Setup(RenderGraphBuilder& builder, RenderPassContext& context, void* pData);
		static void             Execute(RenderPassContext& context, void* pData);
		static void             Destroy(Renderer& renderer, void* pData);
	private:
		ClearDepth() = delete;
	};
}
