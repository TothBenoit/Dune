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
#include <DirectXMath.h>

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
	
	using dSizeT =			size_t;

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

	using dMatrix =			DirectX::XMMATRIX;
	using dMatrix3x3 =		DirectX::XMFLOAT3X3;
	using dMatrix3x4 =		DirectX::XMFLOAT3X4;
	using dMatrix4x3 =		DirectX::XMFLOAT4X3;
	using dMatrix4x4 =		DirectX::XMFLOAT4X4;
	using dQuat =			DirectX::XMVECTOR;
	using dVec =			DirectX::XMVECTOR;
	using dVec2 =			DirectX::XMFLOAT2;
	using dVec3 =			DirectX::XMFLOAT3;
	using dVec4 =	 		DirectX::XMFLOAT4;
	using dVec2u =			DirectX::XMUINT2;
	using dVec3u =			DirectX::XMUINT3;
	using dVec4u =			DirectX::XMUINT4;
	using dVec2i =			DirectX::XMINT2;
	using dVec3i =			DirectX::XMINT3;
	using dVec4i =			DirectX::XMINT4;
}