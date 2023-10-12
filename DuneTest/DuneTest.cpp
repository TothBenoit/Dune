#include <Dune.h>
#include <DirectXMath.h>

using namespace DirectX;

class DuneLauncher : public Dune::Application
{
public:

	void OnStart()
	{
		Dune::EntityID entity = Dune::EngineCore::CreateEntity("My cube");
		Dune::EngineCore::AddComponent<Dune::GraphicsComponent>(entity).mesh = Dune::EngineCore::GetDefaultMesh();
		
		Dune::TransformComponent& cubeTransform = Dune::EngineCore::ModifyComponentUnsafe<Dune::TransformComponent>(entity);
		cubeTransform.position.z = 3.f;
		cubeTransform.position.y = -1.25f;

		Dune::EntityID sunEntity = Dune::EngineCore::CreateEntity("Sun");
		Dune::DirectionalLightComponent& light = Dune::EngineCore::AddComponent<Dune::DirectionalLightComponent>(sunEntity);
		light.intensity = 0.8f;
		Dune::TransformComponent& lightTransform = Dune::EngineCore::ModifyComponentUnsafe<Dune::TransformComponent>(sunEntity);
		lightTransform.rotation.y = 180;

		Dune::Camera& camera = Dune::EngineCore::ModifyCamera();
		camera.rotation.x = 20.f * 3.14f / 180.f;
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