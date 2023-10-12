#pragma once

namespace Dune
{
	class Application
	{
	public:
		Application() = default;
		virtual ~Application() = default;
		DISABLE_COPY_AND_MOVE(Application);

		virtual void OnUpdate(float dt) {};
		virtual void OnStart() {};

		void Start();
	};

}


