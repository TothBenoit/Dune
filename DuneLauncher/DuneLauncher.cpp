#include <Dune.h>

class DuneLauncher : public Dune::Application
{
public:
	DuneLauncher()
	{

	}

	~DuneLauncher()
	{

	}

	void OnUpdate() override
	{
		Dune::LOG_ERROR("WAAAAAAAH")
	}
};

int main(int argc, char** argv)
{
	DuneLauncher* launcher = new DuneLauncher();
	launcher->Start();
	return 0;
}