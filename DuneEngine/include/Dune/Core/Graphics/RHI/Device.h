#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	class Device : public Resource
	{
	public:
		void Initialize();
		void Destroy();

		void* GetInternal() { return m_pInternal; }

		// CopyBufferRegion()
		// CopyTextureRegion()
		// CreateSRV()
		// CreateSampler()
		// CreateRTV()
		// CreateDSV()

	private:
		void* m_pInternal{ nullptr };
	};

}
