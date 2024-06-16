#pragma once

// Disable exception warning
#pragma warning(disable:4530)

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

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <memory>
#include <assert.h>
#include <type_traits>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "Dune/Common/Macro.h"
#include "Dune/Common/Types.h"
#include "Dune/Common/ID.h"