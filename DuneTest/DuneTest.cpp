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
			if (Dune::TransformComponent* cubeTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(entity))
			{
				cubeTransform->position.z = 3.f;
				cubeTransform->position.y = -1.25f;
				cubeTransform->hasChanged = true;
			}

			Dune::EntityID cameraID = Dune::EngineCore::GetCameraID();
			Dune::EngineCore::AddComponent<Dune::PointLightComponent>(cameraID);
			if (Dune::PointLightComponent* light = Dune::EngineCore::GetComponent<Dune::PointLightComponent>(cameraID))
			{
				light->radius = 10.f;
			}
			if (Dune::TransformComponent* camTransform = Dune::EngineCore::GetComponent<Dune::TransformComponent>(cameraID))
			{
				camTransform->rotation.x = 20.f * 3.14f/180.f;
				camTransform->hasChanged = true;
			}

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
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}