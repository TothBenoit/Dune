#pragma once

namespace Dune
{
	class Engine;

	class Application
	{
	public:
		virtual ~Application() = default;

		virtual void Init(Engine& engine) {}
		virtual bool Tick(Engine& engine, float deltaTime) = 0;
		virtual void Shutdown(Engine& engine) {}
	};
}
