#include <Dune.h>

extern "C"
{
	// Used to enable the "Agility SDK" components
	__declspec(dllexport) extern const UINT D3D12SDKVersion = 608 /* D3D12_SDK_VERSION*/;
	__declspec(dllexport) extern const char* D3D12SDKPath = reinterpret_cast<const char*>(u8".\\D3D12\\");
}

class DuneLauncher : public Dune::Application
{
public:
	DuneLauncher()
	{

	}

	~DuneLauncher()
	{

	}

	void OnUpdate(float dt) override
	{

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