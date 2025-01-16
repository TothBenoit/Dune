#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Texture;
	class Buffer;
	struct Descriptor;

	struct SRVDesc
	{
		dU32 mipStart{ 0 };
		dU32 mipLevels{ 1 };
		float mipBias{ 0.0f };
	};

	struct SamplerDesc
	{

	};

	struct RTVDesc
	{
	};

	struct DSVDesc
	{
	};

	class Device : public Resource
	{
	public:
		void Initialize();
		void Destroy();

		void* GetInternal() { return m_pInternal; }

		void CreateSRV(Descriptor& descriptor, Texture& texture, const SRVDesc& desc);
		void CreateSRV(Descriptor& descriptor, Buffer& buffer);
		void CreateSampler(Descriptor& descriptor, const SamplerDesc& desc);
		void CreateRTV(Descriptor& descriptor, Texture& texture, const RTVDesc& desc);
		void CreateDSV(Descriptor& descriptor, Texture& texture, const DSVDesc& desc);

	private:
		void* m_pInternal{ nullptr };
	};

}
