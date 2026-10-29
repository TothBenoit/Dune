#include "pch.h"
#include "Dune/Graphics/ForwardPipeline.h"
#include "Dune/Graphics/RenderPass/SceneResources.h"
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
	RendererDesc MakeDesc()
	{
		RendererDesc desc{};
		desc.Add<SceneResources>();
		desc.Add<Visibility>();
		desc.Add<ClearDepth>();
		desc.Add<MaterialUpload>();
		desc.Add<DepthPrepass>();
		desc.Add<Shadow>();
		desc.Add<LightUpload>();
		desc.Add<ClearHDRTarget>();
		desc.Add<Forward>();
		desc.Add<Tonemapping>();
		return desc;
	}
}
