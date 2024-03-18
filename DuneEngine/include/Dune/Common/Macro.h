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

#define MAX_ENTITIES 200'000