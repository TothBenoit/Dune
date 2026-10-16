#pragma once

#include "Dune/Graphics/Pipeline.h"

namespace Dune::Graphics
{
	class Device;

	using ShaderHandle = dU32;
	inline constexpr ShaderHandle kInvalidShaderHandle{ dU32(-1) };

	using RootSignatureHandle = dU32;
	inline constexpr RootSignatureHandle kInvalidRootSignatureHandle{ dU32(-1) };

	struct GraphicsPSODesc
	{
		ShaderHandle         vertexShader{ kInvalidShaderHandle };
		ShaderHandle         pixelShader{ kInvalidShaderHandle };
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

	struct ComputePSODesc
	{
		ShaderHandle computeShader{ kInvalidShaderHandle };
	};

	bool operator==(const ShaderEntryDesc& a, const ShaderEntryDesc& b);
	bool operator==(const GraphicsPSODesc& a, const GraphicsPSODesc& b);
	bool operator==(const ComputePSODesc& a, const ComputePSODesc& b);

	class PSOCache
	{
	public:
		void Initialize(Device& device);
		void Destroy();

		[[nodiscard]] PSO* Resolve(const PipelineDesc& desc);

	private:
		[[nodiscard]] ShaderHandle ResolveShader(const ShaderEntryDesc& desc);
		// TODO: Derived root signatures from shader reflection or use a fixed common set.
		RootSignatureHandle        ResolveRootSignature(ShaderHandle shader, const RootSignatureDesc& desc);
		[[nodiscard]] PSO*         ResolvePSO(const GraphicsPSODesc& desc);
		[[nodiscard]] PSO*         ResolvePSO(const ComputePSODesc& desc);

		[[nodiscard]] static dU64 Hash(const ShaderEntryDesc& desc);
		[[nodiscard]] static dU64 Hash(const GraphicsPSODesc& desc);
		[[nodiscard]] static constexpr const wchar_t* GetEntryPoint(EShaderStage stage)
		{
			switch (stage)
			{
			case EShaderStage::Vertex:
				return L"VSMain";
			case EShaderStage::Pixel:
				return L"PSMain";
			case EShaderStage::Compute:
				return L"CSMain";
			default:
				Assert(false);
			}
			return L"";
		}

	private:
		struct ShaderEntry
		{
			Shader              shader;
			RootSignatureHandle rootSignature{ kInvalidRootSignatureHandle };
		};

		struct GraphicsPSOEntry
		{
			GraphicsPSODesc desc;
			PSO             pso;
		};

		Device* m_pDevice{ nullptr };
		ShaderCompiler m_compiler;

		dVector<ShaderEntry>          m_shaders;
		dHashMap<dU64, dU32>          m_shaderLookup;
		dDeque<RootSignature>         m_rootSignatures;
		dDeque<GraphicsPSOEntry>      m_graphicsPSOs;
		dHashMap<dU64, dVector<dU32>> m_graphicsPSOLookup;
		dDeque<PSO>                   m_computePSOs;
		dHashMap<dU32, dU32>          m_computePSOLookup;
	};
}
