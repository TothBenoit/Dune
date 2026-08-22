#pragma once
#include "Dune/Core/JobSystem.h"

namespace Dune
{
	struct AsyncLoadRequest
	{
		Job::Counter counter;
		dU64 byteSize;
		void* pData{ nullptr };
	};

	using AsyncRequestID = dU32;
	namespace FileLoader
	{
		AsyncRequestID RequestLoad(const dString& path);
		const AsyncLoadRequest* GetAsyncRequest(AsyncRequestID id);
	}
}	
