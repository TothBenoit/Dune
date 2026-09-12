#include "pch.h"
#include "Dune/Graphics/RHI/PSOCache.h"
#include "Dune/Utilities/HashUtils.h"

namespace Dune::Graphics
{
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
}
