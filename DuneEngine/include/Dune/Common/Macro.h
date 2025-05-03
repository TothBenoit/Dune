#pragma once

#define DISABLE_COPY(ClassName)										\
		ClassName(const ClassName& other) = delete;					\
		ClassName& operator=(const ClassName& other) = delete;

#define DISABLE_MOVE(ClassName)										\
		ClassName(ClassName&& other) = delete;						\
		ClassName& operator=(ClassName&& other) = delete;

#define DISABLE_COPY_AND_MOVE(ClassName)							\
		DISABLE_COPY(ClassName)										\
		DISABLE_MOVE(ClassName)

#ifdef _DEBUG
	#define Assert(condition) do { if (!(condition)) {__debugbreak();} } while(false) 
#else
	#define Assert(condition)
#endif

// #define PROFILE_ENABLED

//#ifdef PROFILE_ENABLED
//	#include <Optick/optick.h>
//	#define ProfileBeginFrame() OPTICK_FRAME("MainThread");
//	#define Profile(msg) OPTICK_EVENT(msg)
//	#define ProfileFunc() OPTICK_EVENT(__func__)
//	#define ProfileBeginThread(threadName) OPTICK_START_THREAD(threadName)
//#else
//	#define Profile(msg)
//#endif