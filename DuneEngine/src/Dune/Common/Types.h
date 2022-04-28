#pragma once

#ifdef DUNE_PLATFORM_WINDOWS
#include <DirectXMath.h>
#endif // DUNE_PLATFORM_WINDOWS

namespace Dune
{
#ifdef DUNE_PLATFORM_WINDOWS
	typedef DirectX::XMMATRIX           dXMMatrix;
	typedef DirectX::XMFLOAT3X3         dMatrix3x3;
	typedef DirectX::XMFLOAT4X3         dMatrix4x3;
	typedef DirectX::XMFLOAT4X4         dMatrix4x4;
	typedef DirectX::XMVECTOR           dXMVector;
	typedef DirectX::XMFLOAT2           dVector2;
	typedef DirectX::XMFLOAT3           dVector3;
	typedef DirectX::XMFLOAT4           dVector4;
	typedef DirectX::XMUINT2            dVector2u;
	typedef DirectX::XMUINT3            dVector3u;
	typedef DirectX::XMUINT4            dVector4u;
	typedef DirectX::XMINT2             dVector2i;
	typedef DirectX::XMINT3             dVector3i;
	typedef DirectX::XMINT4             dVector4i;
#endif // DUNE_PLATFORM_WINDOWS

}