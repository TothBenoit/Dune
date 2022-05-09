#pragma once

#ifdef DUNE_PLATFORM_WINDOWS
	#ifdef DUNE_BUILD_DLL
		#define DUNE_API
	#else
		#define DUNE_API
	#endif
#else
	#error Platform not supported
#endif 