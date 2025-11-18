#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Device;

	enum class EShaderVisibility
	{
		Vertex,
		Pixel,
		All,
		Count
	};

	enum class EBindingType
	{
		Uniform,
		Buffer,
		SRV,
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
			BindingGroup    groupDesc;
			dU32            byteSize;
			dU32            samplerCount;
		};
		EShaderVisibility visibility{ EShaderVisibility::All };
	};


	struct RootSignatureDesc
	{
		dSpan<BindingSlot>  layout;
		bool                bAllowInputLayout{ false };
		bool                bAllowSRVHeapIndexing{ false };
		bool                bAllowSamplerHeapIndexing{ false };
	};

	class RootSignature : public Resource
	{
	public:
		void Initialize(Device* pDevice, const RootSignatureDesc& desc);
		void Destroy();

	};
}