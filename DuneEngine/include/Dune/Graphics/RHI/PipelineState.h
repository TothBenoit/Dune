#pragma once

#include "Dune/Graphics/RHI/Resource.h"
#include "Dune/Graphics/Format.h"

namespace Dune::Graphics
{
	class Device;
	class Shader;
	class RootSignature;

	enum class EBlendFactor : dU8
	{ 
		Zero,
		One,
		SrcAlpha,
		InvSrcAlpha,
		SrcColor,
		InvSrcColor,
		DstAlpha,
		InvDstAlpha
	};

	enum class EBlendOp : dU8
	{ 
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max
	};

	struct BlendState
	{
		EBlendFactor srcColor{ EBlendFactor::SrcAlpha };
		EBlendFactor dstColor{ EBlendFactor::InvSrcAlpha };
		EBlendOp     colorOp { EBlendOp::Add };
		EBlendFactor srcAlpha{ EBlendFactor::One };
		EBlendFactor dstAlpha{ EBlendFactor::InvSrcAlpha };
		EBlendOp     alphaOp { EBlendOp::Add };
		bool         bBlendEnable : 1 { false };
		bool         bRedEnable   : 1 { true };
		bool         bGreenEnable : 1 { true };
		bool         bBlueEnable  : 1 { true };
		bool         bAlphaEnable : 1 { true };
	};

	enum class ECullingMode : dU8
	{
		None  = 1,
		Front = 2,
		Back  = 3
	};

	struct RasterizerState
	{
		dS32         depthBias{ 0 };
		float        depthBiasClamp{ 0.0f };
		float        slopeScaledDepthBias{ 0.0f };
		ECullingMode cullingMode{ ECullingMode::Back };
		bool         bDepthClipEnable : 1 { true };
		bool         bWireframe : 1 { false };
	};

	enum class ECompFunc : dU8
	{
		None,
		Never,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual,
		Always
	};

	struct DepthStencilState
	{
		ECompFunc   bDepthFunc{ ECompFunc::LessEqual };
		bool        bDepthEnabled : 1 { false };
		bool        bDepthWrite   : 1 { false };
		// TODO: Add stencil
	};

	struct VertexInput
	{
		const char* pName{ nullptr };
		dU32 index{ 0 };
		EFormat format{ EFormat::R32G32B32A32_FLOAT };
		dU32 slot{ 0 };
		dU32 byteAlignedOffset{ 0 };
		bool bPerInstance{ false };
	};

	struct GraphicsPipelineDesc
	{
		Shader*                 pVertexShader{ nullptr };
		Shader*                 pPixelShader{ nullptr };

		RootSignature*          pRootSignature{ nullptr };
		dSpan<VertexInput>      inputLayout;
		RasterizerState         rasterizerState;
		DepthStencilState       depthStencilState;

		dU8                     renderTargetCount{ 0 };
		bool                    alphaToCoverageEnable  : 1 { false };
		bool                    independentBlendEnable : 1 { false };
		EFormat                 renderTargetsFormat[8];
		BlendState              renderTargetsBlend[8];
		EFormat                 depthStencilFormat;
	};

	struct ComputePipelineDesc
	{
		Shader*                 pComputeShader{ nullptr };
		RootSignature*          pRootSignature{ nullptr };
	};

	class PipelineState : public Resource
	{
	public:
		void Initialize(Device& device, const GraphicsPipelineDesc& desc);
		void Initialize(Device& device, const ComputePipelineDesc& desc);
		void Destroy();
	};
}
