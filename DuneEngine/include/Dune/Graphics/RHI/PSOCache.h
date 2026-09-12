#pragma once

#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/RHI/RootSignature.h"
#include "Dune/Graphics/RHI/PipelineState.h"
#include "Dune/Core/FileSystem.h"

namespace Dune::Graphics
{
	class Device;

	using ShaderHandle = dU32;
	inline constexpr ShaderHandle kInvalidShaderHandle{ dU32(-1) };

	using RootSignatureHandle = dU32;
	inline constexpr RootSignatureHandle kInvalidRootSignatureHandle{ dU32(-1) };

	using PSOHandle = dU32;
	inline constexpr PSOHandle kInvalidPSOHandle{ dU32(-1) };

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

		[[nodiscard]] ShaderHandle ResolveShader(const ShaderEntryDesc& desc);
		// TODO: Derived root signatures from shader reflection or use a fixed common set.
		RootSignatureHandle        ResolveRootSignature(ShaderHandle shader, const RootSignatureDesc& desc);
		[[nodiscard]] PSOHandle    ResolvePSO(const GraphicsPSODesc& desc);
		[[nodiscard]] PSOHandle    ResolvePSO(const ComputePSODesc& desc);

		[[nodiscard]] PipelineState& GetPipelineState(PSOHandle handle)
		{
			return (handle & kComputePSOFlag) ? m_computePSOs[handle & ~kComputePSOFlag].pso : m_graphicsPSOs[handle].pso;
		}

		[[nodiscard]] RootSignatureHandle GetRootSignatureHandle(PSOHandle handle) const
		{
			return (handle & kComputePSOFlag) ? m_computePSOs[handle & ~kComputePSOFlag].rootSignature : m_graphicsPSOs[handle].rootSignature;
		}

		[[nodiscard]] RootSignature& GetRootSignature(RootSignatureHandle handle) { return m_rootSignatures[handle]; }

	private:
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
			GraphicsPSODesc     desc;
			RootSignatureHandle rootSignature{ kInvalidRootSignatureHandle };
			PipelineState       pso;
		};

		struct ComputePSOEntry
		{
			RootSignatureHandle rootSignature{ kInvalidRootSignatureHandle };
			PipelineState       pso;
		};

		static constexpr PSOHandle kComputePSOFlag{ 1u << 31 };

		Device* m_pDevice{ nullptr };
		void*   m_pCompiler{ nullptr };
		void*   m_pUtils{ nullptr };
		void*   m_pIncludeHandler{ nullptr };

		dVector<ShaderEntry>          m_shaders;
		dHashMap<dU64, dU32>          m_shaderLookup;
		dVector<RootSignature>        m_rootSignatures;
		dVector<GraphicsPSOEntry>     m_graphicsPSOs;
		dHashMap<dU64, dVector<dU32>> m_graphicsPSOLookup;
		dVector<ComputePSOEntry>      m_computePSOs;
		dHashMap<dU32, dU32>          m_computePSOLookup;
	};
}
