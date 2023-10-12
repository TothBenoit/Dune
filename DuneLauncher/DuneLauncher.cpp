#include <Dune.h>

class DuneLauncher : public Dune::Application
{};

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}