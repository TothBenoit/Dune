#pragma once

#include "Dune/Graphics/RHI/Resource.h"
#include "Dune/Graphics/Format.h"

namespace Dune::Graphics
{
	class Device;
	class Shader;

	enum class EShaderVisibility
	{
		Vertex,
		Pixel,
		All,
		Count
	};

	enum class EBindingType
	{
		Constant,
		Buffer,
		Resource,
		UAV,
		Group,
		Samplers,
	};

	struct BindingGroup
	{
		dU32 bufferCount{ 0 };
		dU32 uavCount{ 0 };
		dU32 resourceCount{ 0 };
	};

	struct BindingSlot
	{
		EBindingType type;
		union
		{
			BindingGroup	groupDesc;
			dU32			byteSize;
			dU32			samplerCount;
		};
		EShaderVisibility visibility{ EShaderVisibility::All };
	};

	struct BindingLayout
	{
		BindingSlot slots[64];
		dU8 slotCount{ 0 };
	};

	enum class ECullingMode
	{
		NONE = 1,
		FRONT = 2,
		BACK = 3
	};

	struct RasterizerState
	{
		ECullingMode cullingMode{ ECullingMode::BACK };
		dS32 depthBias{ 0 };
		float depthBiasClamp{ 0.0f };
		float slopeScaledDepthBias{ 0.0f };
		bool bDepthClipEnable : 1 { true };
		bool bWireframe : 1 { false };
	};

	enum class ECompFunc
	{
		NONE,
		NEVER = 1,
		LESS = 2,
		EQUAL = 3,
		LESS_EQUAL = 4,
		GREATER = 5,
		NOT_EQUAL = 6,
		GREATER_EQUAL = 7,
		ALWAYS = 8
	};

	struct DepthStencilState
	{
		bool		bDepthEnabled : 1 { false };
		bool		bDepthWrite : 1	{ false };
		ECompFunc	bDepthFunc{ ECompFunc::LESS_EQUAL };
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
		Shader*					pVertexShader{ nullptr };
		Shader*					pPixelShader{ nullptr };
		BindingLayout			bindingLayout;
		// TODO : Use span
		dVector<VertexInput>	inputLayout;
		RasterizerState			rasterizerState;
		DepthStencilState		depthStencilState;

		dU8						renderTargetCount{ 0 };
		EFormat					renderTargetsFormat[8];
		EFormat					depthStencilFormat;
	};

	class GraphicsPipeline : public Resource
	{
	public:
		void Initialize(Device* pDevice, const GraphicsPipelineDesc& desc);
		void Destroy();
	};
}