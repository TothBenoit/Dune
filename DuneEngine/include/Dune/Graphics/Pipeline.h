#pragma once

#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	enum class EShaderVariant : dU32
	{
		None = 0,
		AlphaMask = 1 << 0,
	};

	inline constexpr const wchar_t* kShaderVariantDefines[]
	{
		L"ALPHA_MASK",
	};

	struct ShaderEntryDesc
	{
		FileSystem::SerializationID<EFileType::Shader> path;
		EShaderStage   stage{ EShaderStage::Vertex };
		EShaderVariant variantMask{ EShaderVariant::None };
	};

	struct PipelineDesc
	{
		ShaderEntryDesc      vertexShader;
		ShaderEntryDesc      pixelShader;
		ShaderEntryDesc      computeShader;
		RootSignatureDesc    rootSignature;
		dVector<VertexInput> inputLayout;
		ECullingMode         cullingMode{ ECullingMode::Back };
		dS32                 depthBias{ 0 };
		float                slopeScaledDepthBias{ 0.0f };
		bool                 depthClipEnable{ true };
		bool                 depthEnabled{ false };
		bool                 depthWrite{ false };
		ECompFunc            depthFunc{ ECompFunc::LessEqual };
		dU8                  renderTargetCount{ 0 };
		EFormat              renderTargetsFormat[8]{};
		bool                 renderTargetsBlendEnable[8]{};
		EFormat              depthStencilFormat{ EFormat::Unknown };
	};

	struct PSO
	{
		PipelineState  pipelineState;
		RootSignature* pRootSignature{ nullptr };
	};
}
