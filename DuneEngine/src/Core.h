#pragma once


#ifdef DUNE_PLATFORM_WINDOWS
	#ifdef DUNE_BUILD_DLL
		#define DUNE_API __declspec(dllexport)
	#else
		#define DUNE_API __declspec(dllimport)
	#endif
#else
	#error No platform defined	
#endif 