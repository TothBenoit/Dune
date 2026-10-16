#include "pch.h"
#include "Dune/Graphics/ForwardPipeline.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RenderPass/Visibility.h"
#include "Dune/Graphics/RenderPass/ClearDepth.h"
#include "Dune/Graphics/RenderPass/ClearHDRTarget.h"
#include "Dune/Graphics/RenderPass/MaterialUpload.h"
#include "Dune/Graphics/RenderPass/DepthPrepass.h"
#include "Dune/Graphics/RenderPass/Shadow.h"
#include "Dune/Graphics/RenderPass/LightUpload.h"
#include "Dune/Graphics/RenderPass/Forward.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"

namespace Dune::Graphics::ForwardPipeline
{
	void Register(Renderer& renderer)
	{
		renderer.RegisterRenderPass<Visibility>();
		renderer.RegisterRenderPass<ClearDepth>();
		renderer.RegisterRenderPass<MaterialUpload>();
		renderer.RegisterRenderPass<DepthPrepass>();
		renderer.RegisterRenderPass<Shadow>();
		renderer.RegisterRenderPass<LightUpload>();
		renderer.RegisterRenderPass<ClearHDRTarget>();
		renderer.RegisterRenderPass<Forward>();
		renderer.RegisterRenderPass<Tonemapping>();
	}
}
