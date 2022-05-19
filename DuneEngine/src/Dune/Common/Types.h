#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

#ifdef DUNE_PLATFORM_WINDOWS
#include <DirectXMath.h>
#endif // DUNE_PLATFORM_WINDOWS

namespace Dune
{
	using dU64 =			uint64_t;
	using dU32 =			uint32_t;
	using dU16 =			uint16_t;
	using dU8 =				uint8_t;
	using dUInt =			uint32_t;

	using dS64 =			int64_t;
	using dS32 =			int32_t;
	using dS16 =			int16_t;
	using dS8 =				int8_t;
	using dInt =			int32_t;

	using dString =			std::string;
	using dWString =		std::wstring;

	template<typename T>	
	using dVector =			std::vector<T>;

	template<typename T>
	using dList =			std::list<T>;

	template<typename T>
	using dQueue =			std::queue<T>;

	template<typename T>
	using dDeque =			std::deque<T>;

	template<typename T>
	using dStack =			std::stack<T>;

	template<typename K, typename T>
	using dMap =			std::map<K, T>;

	template<typename K, typename T>
	using dHashMap =		std::unordered_map <K, T> ;

	template<typename K>
	using dSet =			std::set <K>;

	template<typename K>
	using dHashSet =		std::unordered_set <K>;

#ifdef DUNE_PLATFORM_WINDOWS
	typedef DirectX::XMMATRIX           dMatrix;
	typedef DirectX::XMFLOAT3X3         dMatrix3x3;
	typedef DirectX::XMFLOAT4X3         dMatrix4x3;
	typedef DirectX::XMFLOAT4X4         dMatrix4x4;
	typedef DirectX::XMVECTOR           dVec;
	typedef DirectX::XMFLOAT2           dVec2;
	typedef DirectX::XMFLOAT3           dVec3;
	typedef DirectX::XMFLOAT4           dVec4;
	typedef DirectX::XMUINT2            dVec2u;
	typedef DirectX::XMUINT3            dVec3u;
	typedef DirectX::XMUINT4            dVec4u;
	typedef DirectX::XMINT2             dVec2i;
	typedef DirectX::XMINT3             dVec3i;
	typedef DirectX::XMINT4             dVec4i;
#endif // DUNE_PLATFORM_WINDOWS
}