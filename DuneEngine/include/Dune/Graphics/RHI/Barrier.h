#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
    enum class EResourceState : dU32
    {
        Undefined = 0,
        ShaderResource = 1 << 0,
        ShaderResourceNonPixel = 1 << 1,
        UAV = 1 << 2,
        CopySource = 1 << 3,
        CopyDest = 1 << 4,

        RenderTarget = 1 << 5,
        DepthStencil = 1 << 6,
        DepthStencilRead = 1 << 7,

        VertexBuffer = 1 << 9,
        IndexBuffer = 1 << 10,
        ConstantBuffer = 1 << 11,
        IndirectArgument = 1 << 12,

        Present = 1 << 13
    };

	class Barrier : public Resource
	{
    public:
		void Initialize(dU32 barrierCapacity);
		void Destroy();

        void PushTransition(void* pResource, EResourceState stateBefore, EResourceState stateAfter);
        void Reset() { m_barrierCount = 0; }

        [[nodiscard]] dU32 GetBarrierCount() const { return m_barrierCount; }
    private:
        dU32 m_barrierCount{ 0 };
        dU32 m_barrierCapacity;
	};
}