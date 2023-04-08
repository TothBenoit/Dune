#ifndef DUNE_API
	#define DUNE_API extern "C" __declspec(dllexport)
#endif // !DUNE_API

#include "Dune.h"

using namespace Dune;

DUNE_API ID::IDType CreateEntity(const dString& name)
{
	return EngineCore::CreateEntity(name);
}