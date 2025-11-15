#pragma once

#include "Dune/Graphics/RHI/Resource.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"

namespace Dune::Graphics
{
	class Texture;
	class Buffer;

	enum class ESRVDimension
	{
		Buffer,
		Texture1D,
		Texture1DArray,
		Texture2D,
		Texture2DArray,
		Texture3D,
		TextureCube,
		TextureCubeArray
	};

	struct SRVDesc
	{
		dU32 mipStart{ 0 };
		dU32 mipLevels{ 1 };
		float mipBias{ 0.0f };
		dU32 firstArraySlice{ 0 };
		dU32 arraySize{ (dU32)-1 };
		EFormat	format{ EFormat::R8G8B8A8_UNORM };
		ESRVDimension dimension{ ESRVDimension::Texture2D };
	};

	struct SamplerDesc
	{

	};

	struct RTVDesc
	{
	};

	enum class EDSVDimension
	{
		Texture1D,
		Texture1DArray,
		Texture2D,
		Texture2DArray,
	};

	struct DSVDesc
	{
		dU32 mipSlice{ 0 };
		dU32 firstArraySlice{ 0 };
		dU32 arraySize{ (dU32)-1 };
		EDSVDimension dimension{ EDSVDimension::Texture2D };
	};

	class Device : public Resource
	{
	public:
		void Initialize();
		void Destroy();

		void* GetInternal() { return m_pInternal; }

		void CreateSRV(Descriptor& descriptor, Texture& texture, const SRVDesc& desc);
		void CreateSRV(Descriptor& descriptor, Texture& texture);
		void CreateSRV(Descriptor& descriptor, Buffer& buffer);
		void CreateSampler(Descriptor& descriptor, const SamplerDesc& desc);
		void CreateRTV(Descriptor& descriptor, Texture& texture, const RTVDesc& desc);
		void CreateDSV(Descriptor& descriptor, Texture& texture, const DSVDesc& desc);

		void CopyDescriptors(dU32 count, dU64 cpuAddressSrc, dU64 cpuAddressDst, EDescriptorHeapType type);

	private:
		void* m_pInternal{ nullptr };
	};

}
