#pragma once

#include "Dune/Platform/RHI/Shader.h"
#include "Dune/Platform/RHI/RootSignature.h"
#include "Dune/Platform/RHI/PipelineState.h"
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
		PathID<RHI::Shader> path;
		RHI::EShaderStage stage{ RHI::EShaderStage::Vertex };
		EShaderVariant variantMask{ EShaderVariant::None };
	};

	struct PipelineDesc
	{
		ShaderEntryDesc           vertexShader;
		ShaderEntryDesc           pixelShader;
		ShaderEntryDesc           computeShader;
		RHI::RootSignatureDesc    rootSignature;
		dVector<RHI::VertexInput> inputLayout;
		RHI::ECullingMode         cullingMode{ RHI::ECullingMode::Back };
		dS32                      depthBias{ 0 };
		float                     slopeScaledDepthBias{ 0.0f };
		bool                      depthClipEnable{ true };
		bool                      depthEnabled{ false };
		bool                      depthWrite{ false };
		RHI::ECompFunc            depthFunc{ RHI::ECompFunc::LessEqual };
		dU8                       renderTargetCount{ 0 };
		RHI::EFormat              renderTargetsFormat[8]{};
		bool                      renderTargetsBlendEnable[8]{};
		RHI::EFormat              depthStencilFormat{ RHI::EFormat::Unknown };
	};

	struct PSO
	{
		RHI::PipelineState  pipelineState;
		RHI::RootSignature* pRootSignature{ nullptr };
	};
}
