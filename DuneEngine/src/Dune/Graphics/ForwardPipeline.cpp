#include "pch.h"
#include "Dune/Graphics/ForwardPipeline.h"
#include "Dune/Graphics/Renderer.h"
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
	struct PipelineData
	{
		SceneResourcesData sceneResources;
		VisibilityData visibility;
		MaterialUploadData* pMaterialUpload{ nullptr };
		ShadowPassData* pShadow{ nullptr };
		LightUploadData* pLightUpload{ nullptr };
		TonemappingData* pTonemapping{ nullptr };
	};

	static void* Create(Renderer& renderer)
	{
		PipelineData* pData = new PipelineData();
		SceneResources::Initialize(renderer, pData->sceneResources);
		renderer.RegisterPass<ClearDepth>();
		pData->pMaterialUpload = renderer.RegisterPass<MaterialUpload>();
		renderer.RegisterPass<DepthPrepass>();
		pData->pShadow = renderer.RegisterPass<Shadow>();
		pData->pLightUpload = renderer.RegisterPass<LightUpload>();
		renderer.RegisterPass<ClearHDRTarget>();
		renderer.RegisterPass<Forward>();
		pData->pTonemapping = renderer.RegisterPass<Tonemapping>();
		return pData;
	}

	static void Setup(RenderPassContext& context, void* pPipelineData)
	{
		PipelineData* pData = static_cast<PipelineData*>(pPipelineData);
		SceneResources::Setup(context, pData->sceneResources);
		Visibility::Compute(context, pData->visibility);
		context.SetupPass<ClearDepth>(nullptr);
		context.SetupPass<MaterialUpload>(pData->pMaterialUpload);
		context.SetupPass<DepthPrepass>(nullptr);
		context.SetupPass<Shadow>(pData->pShadow);
		context.SetupPass<LightUpload>(pData->pLightUpload);
		context.SetupPass<ClearHDRTarget>(nullptr);
		context.SetupPass<Forward>(nullptr);
		context.SetupPass<Tonemapping>(pData->pTonemapping);
	}

	static void Destroy(Renderer&, void* pPipelineData)
	{
		delete static_cast<PipelineData*>(pPipelineData);
	}

	RendererDesc MakeDesc()
	{
		return { .pCreate = &Create, .pSetup = &Setup, .pDestroy = &Destroy };
	}
}
