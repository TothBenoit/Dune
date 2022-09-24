#pragma once

namespace Dune
{
	class Application
	{
	public:
		virtual void OnUpdate(float dt) = 0;

		void Start();
	};

}


