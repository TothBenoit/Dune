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
	#define Assert(condition) if (!(condition)) {__debugbreak();} 
#else
	#define Assert(condition)
#endif

#define RESERVED_ENTITIES 4096