#pragma once

#pragma comment(lib,"d3d12.lib") 
#pragma comment(lib,"dxgi.lib") 
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <dxgidebug.h>
#include <d3dx12/d3dx12.h>
#include <d3d12shader.h>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 613; }
extern "C" { __declspec(dllexport) extern const char8_t* D3D12SDKPath = u8".\\D3D12\\"; }

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}

#ifdef _DEBUG
#define NameDXObject(obj, name) if (name) { obj->SetName(name); OutputDebugString(name); OutputDebugString(L" has been created \n"); }
#define NameDXObjectIndexed(obj, n, name)						\
		{																\
			wchar_t indexedName[128];									\
			if (swprintf_s(indexedName, L"%s[%u]", name, n) > 0)		\
			{															\
				obj->SetName(indexedName);								\
				OutputDebugString(indexedName);							\
				OutputDebugString(L" has been created \n");				\
			}															\
		}
#else // !_DEBUG
#define NameDXObject(obj, name)
#define NameDXObjectIndexed(obj, n, name)
#endif //_DEBUG 