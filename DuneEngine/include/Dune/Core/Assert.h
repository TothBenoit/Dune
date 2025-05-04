#pragma once

#ifdef _DEBUG
	#define Assert(condition) do { if (!(condition)) {__debugbreak();} } while(false) 
#else
	#define Assert(condition)
#endif