#pragma once
#include "Core.h"

namespace Dune
{
	class DUNE_API Application
	{
	public:
		Application();
		~Application();

		virtual void OnUpdate() = 0;

		void Start();

	protected:
		bool m_Running;
	};

}


