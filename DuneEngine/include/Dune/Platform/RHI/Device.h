#pragma once

#include "Dune/Platform/RHI/Resource.h"
#include "Dune/Platform/RHI/DescriptorHeap.h"
#include "Dune/Platform/RHI/Format.h"
#include "Dune/Platform/RHI/PhysicalDevice.h"
#include "Dune/Platform/RHI/Texture.h"

namespace Dune::RHI
{
	class Texture;
	class Buffer;

	struct SRVTextureDesc
	{
		dU32 mipStart{ 0 };
		dU32 mipLevels{ 1 };
		float mipBias{ 0.0f };
		dU32 firstArraySlice{ 0 };
		dU32 arraySize{ (dU32)-1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		ETextureDimension dimension{ ETextureDimension::Texture2D };

		[[nodiscard]] bool operator==(const SRVTextureDesc&) const = default;
	};

	struct SRVBufferDesc
	{
		EFormat	format{ EFormat::Unknown };
		dU64 firstElement{ 0 };
		dU32 elementCount{ 0 };
		dU32 byteStride{ 0 };

		[[nodiscard]] bool operator==(const SRVBufferDesc&) const = default;
	};

	struct UAVBufferDesc
	{
		EFormat	format{ EFormat::Unknown };
		dU64 firstElement{ 0 };
		dU32 elementCount{ 0 };
		dU32 byteStride{ 0 };

		[[nodiscard]] bool operator==(const UAVBufferDesc&) const = default;
	};

	struct SamplerDesc
	{

	};

	struct RTVDesc
	{
		dU32 mipSlice{ 0 };
		dU32 firstArraySlice{ 0 };
	};

	enum class EDSVAccess : dU8
	{
		Write = 0,
		ReadDepth = 1 << 0,
		ReadStencil = 1 << 1,
		ReadDepthStencil = ReadDepth | ReadStencil,
	};

	struct DSVDesc
	{
		dU32 mipSlice{ 0 };
		dU32 firstArraySlice{ 0 };
		EDSVAccess access{ EDSVAccess::Write };
	};

	class Device : public Resource
	{
	public:
		void Initialize(const PhysicalDevice& physicalDevice);
		void Destroy();

		void* GetInternal() { return m_pInternal; }

		void CreateSRV(Descriptor descriptor, Texture& texture, const SRVTextureDesc& desc);
		void CreateSRV(Descriptor descriptor, Texture& texture);
		void CreateSRV(Descriptor descriptor, Buffer& buffer, const SRVBufferDesc& desc);
		void CreateUAV(Descriptor descriptor, Buffer& buffer, const UAVBufferDesc& desc);
		void CreateSampler(Descriptor descriptor, const SamplerDesc& desc);
		void CreateRTV(Descriptor descriptor, Texture& texture, const RTVDesc& desc);
		void CreateDSV(Descriptor descriptor, Texture& texture, const DSVDesc& desc);

		void CopyDescriptors(dU32 count, dU64 cpuAddressSrc, dU64 cpuAddressDst, EDescriptorHeapType type);

	private:
		void* m_pInternal{ nullptr };
	};

}
