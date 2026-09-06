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

	enum class EColorMask : dU8
	{
		None = 0,
		R    = 1 << 0,
		G    = 1 << 1,
		B    = 1 << 2,
		A    = 1 << 3,
		RG   = R | G,
		RB   = R | B,
		RA   = R | A,
		RGB  = R | G | B,
		RGA  = R | G | A,
		RBA  = R | B | A,
		RGBA = R | B | G | A,
		GB   = G | B,
		GA   = G | A,
		GBA  = G | B | A,
		BA   = B | A,
		All  = RGBA
	};

	inline EColorMask operator|(EColorMask a, EColorMask b)
	{
		return EColorMask(dU8(a) | dU8(b));
	}

	inline EColorMask operator&(EColorMask a, EColorMask b)
	{
		return EColorMask(dU8(a) & dU8(b));
	}

	inline bool HasRed  (EColorMask mask) { return (mask & EColorMask::R) != EColorMask::None; }
	inline bool HasGreen(EColorMask mask) { return (mask & EColorMask::G) != EColorMask::None; }
	inline bool HasBlue (EColorMask mask) { return (mask & EColorMask::B) != EColorMask::None; }
	inline bool HasAlpha(EColorMask mask) { return (mask & EColorMask::A) != EColorMask::None; }

	struct BlendState
	{
		EBlendFactor colorSrc        { EBlendFactor::SrcAlpha };
		EBlendFactor colorDst        { EBlendFactor::InvSrcAlpha };
		EBlendOp     colorOp         { EBlendOp::Add };
		EBlendFactor alphaSrc        { EBlendFactor::One };
		EBlendFactor alphaDst        { EBlendFactor::InvSrcAlpha };
		EBlendOp     alphaOp         { EBlendOp::Add };
		EColorMask   colorMask       { EColorMask::RGBA };
		bool         blendEnable : 1 { false };
	};

	enum class ECullingMode : dU8
	{
		None  = 1,
		Front = 2,
		Back  = 3
	};

	struct RasterizerState
	{
		dS32         depthBias            { 0 };
		float        depthBiasClamp       { 0.0f };
		float        slopeScaledDepthBias { 0.0f };
		ECullingMode cullingMode          { ECullingMode::Back };
		bool         depthClipEnable : 1  { true };
		bool         isWireframe     : 1  { false };
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
		ECompFunc   depthFunc{ ECompFunc::LessEqual };
		bool        depthEnabled : 1 { false };
		bool        depthWrite   : 1 { false };
		// TODO: Add stencil
	};

	struct VertexInput
	{
		const char* pName{ nullptr };
		dU32 index{ 0 };
		EFormat format{ EFormat::R32G32B32A32_FLOAT };
		dU32 slot{ 0 };
		dU32 byteAlignedOffset{ 0 };
		bool isPerInstance{ false };
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
