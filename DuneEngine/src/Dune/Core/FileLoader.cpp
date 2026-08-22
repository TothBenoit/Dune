#include "pch.h"
#include "Dune/Core/FileLoader.h"
#include "Dune/Core/File.h"

namespace Dune::FileLoader
{
	constexpr dU32 kRequestPoolSize = 256;
	static_assert((kRequestPoolSize& (kRequestPoolSize - 1)) == 0);

	std::mutex g_requestLock;
	AsyncLoadRequest g_requestPool[kRequestPoolSize];

	std::mutex g_extraRequestLock;
	std::vector<AsyncLoadRequest*> g_extraRequests;
	dU32 g_tail = kRequestPoolSize - 1;
	dU32 g_head = 0;
	dU32 g_extraRequestCounter = 0;

	AsyncRequestID RequestLoad(const dString& path)
	{
		AsyncRequestID id;
		AsyncLoadRequest* pRequest{ nullptr };
		g_requestLock.lock();
		if (g_tail != g_head)
		{
			pRequest = &g_requestPool[g_head];
			id = g_head;
			g_head = (g_head + 1) & (kRequestPoolSize - 1);
		}
		g_requestLock.unlock();
		if (pRequest == nullptr)
		{
			g_extraRequestLock.lock();
			pRequest = g_extraRequests.emplace_back(new AsyncLoadRequest());
			id = kRequestPoolSize + g_extraRequestCounter++;
			g_extraRequestLock.unlock();
		}

		Job::JobBuilder builder;
		builder.DispatchJob([&]() {
			File file;
			if (!File::Open(file, path.c_str(), File::EAccessMode::Read, File::EShareMode::None))
				return;

			dU64 byteSize = file.GetByteSize();
			dU8* pFileBuffer = new dU8[byteSize];
			if (!file.Read(reinterpret_cast<char*>(pFileBuffer), byteSize))
			{
				delete[] pFileBuffer;
				file.Close();
				return;
			}
			
			file.Close();
			pRequest->byteSize = byteSize;
			pRequest->pData = pFileBuffer;
		});

		pRequest->counter = builder.ExtractWaitCounter();
		return id;
	}

	const AsyncLoadRequest* GetAsyncRequest(AsyncRequestID id)
	{
		if (id < kRequestPoolSize)
			return &g_requestPool[id];
		dU32 requestIndex = id - kRequestPoolSize;
		g_extraRequestLock.lock();
		const AsyncLoadRequest* pRequest = requestIndex < g_extraRequests.size() ? g_extraRequests[requestIndex] : nullptr;
		g_extraRequestLock.unlock();
		return pRequest;
	}
}
