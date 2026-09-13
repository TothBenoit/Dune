#include "pch.h"
#include "Dune/Graphics/PSOCache.h"
#include "Dune/Utilities/HashUtils.h"
#include "Dune/Utilities/StringUtils.h"

namespace Dune::Graphics
{
	void PSOCache::Initialize(Device& device)
	{
		m_pDevice = &device;
		m_compiler.Initialize();
	}

	void PSOCache::Destroy()
	{
		for (GraphicsPSOEntry& entry : m_graphicsPSOs)
			entry.pso.Destroy();
		for (ComputePSOEntry& entry : m_computePSOs)
			entry.pso.Destroy();
		for (RootSignature& rootSignature : m_rootSignatures)
			rootSignature.Destroy();
		for (ShaderEntry& entry : m_shaders)
			entry.shader.Destroy();

		m_graphicsPSOs.clear();
		m_graphicsPSOLookup.clear();
		m_computePSOs.clear();
		m_computePSOLookup.clear();
		m_rootSignatures.clear();
		m_shaders.clear();
		m_shaderLookup.clear();

		m_compiler.Destroy();
		m_pDevice = nullptr;
	}

	dU64 PSOCache::Hash(const ShaderEntryDesc& desc)
	{
		constexpr dU32 pathBitSize    = sizeof(desc.path.index) * 8;
		constexpr dU32 stageBitSize   = sizeof(desc.stage) * 8;
		constexpr dU32 variantBitSize = _countof(kShaderVariantDefines);
		static_assert(pathBitSize + stageBitSize + variantBitSize <= 64);
		return (dU64)desc.path.index | ((dU64)desc.stage << pathBitSize) | ((dU64)desc.variantMask << (stageBitSize + pathBitSize));
	}

	bool operator==(const ShaderEntryDesc& a, const ShaderEntryDesc& b)
	{
		return a.path.index == b.path.index && a.stage == b.stage && a.variantMask == b.variantMask;
	}

	dU64 PSOCache::Hash(const GraphicsPSODesc& desc)
	{
		dU64 hash{ HashUtils::kHashOffsetBasis };
		HashUtils::HashValue(hash, desc.vertexShader);
		HashUtils::HashValue(hash, desc.pixelShader);
		for (const VertexInput& input : desc.inputLayout)
		{
			HashUtils::HashBytes(hash, input.pName, strlen(input.pName) + 1);
			HashUtils::HashValue(hash, input.index);
			HashUtils::HashValue(hash, input.format);
			HashUtils::HashValue(hash, input.slot);
			HashUtils::HashValue(hash, input.byteAlignedOffset);
			HashUtils::HashValue(hash, input.isPerInstance);
		}
		HashUtils::HashValue(hash, desc.cullingMode);
		HashUtils::HashValue(hash, desc.depthBias);
		HashUtils::HashValue(hash, desc.slopeScaledDepthBias);
		HashUtils::HashValue(hash, desc.depthClipEnable);
		HashUtils::HashValue(hash, desc.depthEnabled);
		HashUtils::HashValue(hash, desc.depthWrite);
		HashUtils::HashValue(hash, desc.depthFunc);
		HashUtils::HashValue(hash, desc.renderTargetCount);
		for (dU8 i = 0; i < desc.renderTargetCount; i++)
		{
			HashUtils::HashValue(hash, desc.renderTargetsFormat[i]);
			HashUtils::HashValue(hash, desc.renderTargetsBlendEnable[i]);
		}
		HashUtils::HashValue(hash, desc.depthStencilFormat);
		return hash;
	}

	bool operator==(const GraphicsPSODesc& a, const GraphicsPSODesc& b)
	{
		if (a.vertexShader != b.vertexShader || a.pixelShader != b.pixelShader || a.inputLayout.size() != b.inputLayout.size()
			|| a.cullingMode != b.cullingMode || a.depthBias != b.depthBias || a.slopeScaledDepthBias != b.slopeScaledDepthBias
			|| a.depthClipEnable != b.depthClipEnable || a.depthEnabled != b.depthEnabled || a.depthWrite != b.depthWrite
			|| a.depthFunc != b.depthFunc || a.renderTargetCount != b.renderTargetCount || a.depthStencilFormat != b.depthStencilFormat)
			return false;

		for (dSizeT i = 0; i < a.inputLayout.size(); i++)
		{
			const VertexInput& inputA = a.inputLayout[i];
			const VertexInput& inputB = b.inputLayout[i];
			if (strcmp(inputA.pName, inputB.pName) != 0 || inputA.index != inputB.index || inputA.format != inputB.format
				|| inputA.slot != inputB.slot || inputA.byteAlignedOffset != inputB.byteAlignedOffset || inputA.isPerInstance != inputB.isPerInstance)
				return false;
		}

		for (dU8 i = 0; i < a.renderTargetCount; i++)
		{
			if (a.renderTargetsFormat[i] != b.renderTargetsFormat[i] || a.renderTargetsBlendEnable[i] != b.renderTargetsBlendEnable[i])
				return false;
		}

		return true;
	}

	bool operator==(const ComputePSODesc& a, const ComputePSODesc& b)
	{
		return a.computeShader == b.computeShader;
	}

	ShaderHandle PSOCache::ResolveShader(const ShaderEntryDesc& desc)
	{
		dU64 key = Hash(desc);
		auto it = m_shaderLookup.find(key);
		if (it != m_shaderLookup.end())
			return it->second;

		Assert((dU32(desc.variantMask) >> _countof(kShaderVariantDefines)) == 0);
		dVector<const wchar_t*> args{};
		args.reserve(_countof(kShaderVariantDefines));
		for (dU32 bit = 0; bit < _countof(kShaderVariantDefines); bit++)
		{
			if (dU32(desc.variantMask) & (1u << bit))
			{
				args.push_back(L"-D");
				args.push_back(kShaderVariantDefines[bit]);
			}
		}

		const dWString path = StringUtils::ToWide(FileSystem::GetPath(desc.path));

		Shader shader = m_compiler.CompileShader
		({
			.stage = desc.stage,
			.filePath = path.c_str(),
			.entryFunc = GetEntryPoint(desc.stage),
			.args = args.data(),
			.argsCount = (dU32)args.size()
			});

		const dU32 index = (dU32)m_shaders.size();
		ShaderEntry& entry = m_shaders.emplace_back();
		entry.shader = shader;
		m_shaderLookup[key] = index;
		return index;
	}

	RootSignatureHandle PSOCache::ResolveRootSignature(ShaderHandle shader, const RootSignatureDesc& desc)
	{
		ShaderEntry& entry = m_shaders[shader];
		if (entry.rootSignature == kInvalidRootSignatureHandle)
		{
			entry.rootSignature = (RootSignatureHandle)m_rootSignatures.size();
			m_rootSignatures.emplace_back().Initialize(*m_pDevice, desc);
		}
		return entry.rootSignature;
	}

	PSOHandle PSOCache::ResolvePSO(const GraphicsPSODesc& desc)
	{
		dVector<dU32>& candidates = m_graphicsPSOLookup[Hash(desc)];
		for (dU32 index : candidates)
		{
			if (m_graphicsPSOs[index].desc == desc)
				return index;
		}

		const bool hasPixelShader = desc.pixelShader != kInvalidShaderHandle;
		const RootSignatureHandle rootSignature = m_shaders[hasPixelShader ? desc.pixelShader : desc.vertexShader].rootSignature;
		Assert(rootSignature != kInvalidRootSignatureHandle);

		const dU32 index = (dU32)m_graphicsPSOs.size();
		GraphicsPSOEntry& entry = m_graphicsPSOs.emplace_back();
		entry.desc = desc;
		entry.rootSignature = rootSignature;

		GraphicsPipelineDesc pipelineDesc
		{
			.pVertexShader = &m_shaders[desc.vertexShader].shader,
			.pPixelShader = hasPixelShader ? &m_shaders[desc.pixelShader].shader : nullptr,
			.pRootSignature = &m_rootSignatures[rootSignature],
			.inputLayout = entry.desc.inputLayout,
			.rasterizerState =
			{
				.depthBias = desc.depthBias,
				.slopeScaledDepthBias = desc.slopeScaledDepthBias,
				.cullingMode = desc.cullingMode,
				.depthClipEnable = desc.depthClipEnable,
			},
			.depthStencilState =
			{
				.depthFunc = desc.depthFunc,
				.depthEnabled = desc.depthEnabled,
				.depthWrite = desc.depthWrite,
			},
			.renderTargetCount = desc.renderTargetCount,
			.depthStencilFormat = desc.depthStencilFormat,
		};

		for (dU8 i = 0; i < desc.renderTargetCount; i++)
		{
			pipelineDesc.renderTargetsFormat[i] = desc.renderTargetsFormat[i];
			pipelineDesc.renderTargetsBlend[i].blendEnable = desc.renderTargetsBlendEnable[i];
		}

		entry.pso.Initialize(*m_pDevice, pipelineDesc);
		candidates.push_back(index);
		return index;
	}

	PSOHandle PSOCache::ResolvePSO(const ComputePSODesc& desc)
	{
		auto it = m_computePSOLookup.find(desc.computeShader);
		if (it != m_computePSOLookup.end())
			return it->second | kComputePSOFlag;

		const RootSignatureHandle rootSignature = m_shaders[desc.computeShader].rootSignature;
		Assert(rootSignature != kInvalidRootSignatureHandle);

		const dU32 index = (dU32)m_computePSOs.size();
		ComputePSOEntry& entry = m_computePSOs.emplace_back();
		entry.rootSignature = rootSignature;
		entry.pso.Initialize(*m_pDevice, ComputePipelineDesc{ .pComputeShader = &m_shaders[desc.computeShader].shader, .pRootSignature = &m_rootSignatures[rootSignature] });

		m_computePSOLookup[desc.computeShader] = index;
		return index | kComputePSOFlag;
	}
}
