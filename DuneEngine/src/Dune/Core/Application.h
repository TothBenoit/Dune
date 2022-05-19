#pragma once
#include "Core.h"

namespace Dune
{
	class DUNE_API Application
	{
	public:
		Application();
		~Application();

		virtual void OnUpdate(float dt) = 0;

		void Start();
	};

}


