#include "pch.h"
#include "Dune/Graphics/RenderPass/Tonemapping.h"
#include "Dune/Graphics/Shaders/ShaderTypes.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/Format.h"
#include "Dune/Graphics/Mesh.h"
#include "Dune/Scene/Scene.h"
#include "Dune/Scene/Camera.h"

namespace Dune::Graphics
{
	void Tonemapping::Initialize(Device* pDevice)
	{
		m_pDevice = pDevice;
		const wchar_t* args[] = { L"-all_resources_bound", L"-Zi", L"-Qembed_debug" };

		Shader fullScreenTriangleVS;
		fullScreenTriangleVS.Initialize
		({
			.stage = EShaderStage::Vertex,
			.filePath = L"Shaders\\FullScreenTriangle.hlsl",
			.entryFunc = L"VSMain",
			.args = args,
			.argsCount = _countof(args),
			});


		Shader tonemappingPS;
		tonemappingPS.Initialize
		({
			.stage = EShaderStage::Pixel,
			.filePath = L"Shaders\\Tonemapping.hlsl",
			.entryFunc = L"PSMain",
			.args = args,
			.argsCount = _countof(args),
			});

		m_rootSignature.Initialize(pDevice,
			{
				.layout =
				{
					{.type = EBindingType::Group, .groupDesc = {.resourceCount = 1}, .visibility = EShaderVisibility::Pixel},
				},
			});

		m_pipeline.Initialize(pDevice,
			{
				.pVertexShader = &fullScreenTriangleVS,
				.pPixelShader = &tonemappingPS,
				.pRootSignature = &m_rootSignature,
				.renderTargetCount = 1,
				.renderTargetsFormat = { EFormat::R8G8B8A8_UNORM },
			});

		fullScreenTriangleVS.Destroy();
		tonemappingPS.Destroy();
	}

	void Tonemapping::Destroy()
	{
		m_pipeline.Destroy();
		m_rootSignature.Destroy();
	}

	void Tonemapping::Render(CommandList& commandList, Descriptor& source)
	{
		commandList.SetGraphicsRootSignature(m_rootSignature);
		commandList.SetPipelineState(m_pipeline);
		commandList.SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
		commandList.BindGraphicsResource(0, source);
		commandList.DrawInstanced(3, 1, 0, 0);
	}
}