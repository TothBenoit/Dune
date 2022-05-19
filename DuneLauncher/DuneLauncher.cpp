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

	void OnUpdate(float dt) override
	{

	}
};

int main(int argc, char** argv)
{
	DuneLauncher launcher = DuneLauncher();
	launcher.Start();
	return 0;
}