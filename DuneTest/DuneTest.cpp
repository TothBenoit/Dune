#include <Dune.h>
#include <DirectXMath.h>


using namespace DirectX;

class DuneLauncher : public Dune::Application
{
public:
	DuneLauncher()
	{

	}

	~DuneLauncher()
	{

	}

	void Test1()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			srand((unsigned int)time(NULL));

			for (int i = 0; i < 1000; i++)
			{
				Dune::EntityID entity = Dune::EngineCore::CreateEntity("Node");
				if ((rand() % 11) < 3)
				{
					Dune::EngineCore::RemoveEntity(entity);
				}
			}
			didTestOnce = true;
		}
	}

	void Test2()
	{
		static bool didTestOnce = false;
		if (!didTestOnce)
		{
			Dune::EntityID entity = Dune::EngineCore::CreateEntity("My cube");
			Dune::EngineCore::AddComponent<Dune::GraphicsComponent>(entity);

			didTestOnce = true;
		}
	}

	void OnUpdate(float dt) override
	{
		//Test1();
		Test2();
	}

};

int main(int argc, char** argv)
{
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}