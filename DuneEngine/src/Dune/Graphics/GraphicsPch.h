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
#include <imgui/imgui_impl_dx12.h>

#ifdef _DEBUG
#define NameDXObject(obj, name) obj->SetName(name); OutputDebugString(name); OutputDebugString(L" has been created \n");
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