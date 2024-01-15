#include <Dune.h>

using namespace Dune;

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	Graphics::Initialize();
	Graphics::Device* pDevice = Graphics::CreateDevice();
	dVector<Graphics::View*> views;
	dU32 viewCount{ 5 };
	views.reserve(viewCount);

	for (dU32 i{ 0 }; i < viewCount; i++)
	{
		views.push_back(Graphics::CreateView({ .pDevice= pDevice }));
	}

	bool running = true;
	while (running)
	{
		for (auto it = views.begin(); it != views.end();)
		{
			if (!Graphics::ProcessViewEvents(*it))
			{
				Graphics::DestroyView(*it);
				it = views.erase(it);
				continue;
			}
			++it;
		}
		running = !views.empty();
		// Game goes here
	}

	Graphics::DestroyDevice(pDevice);
	Graphics::Shutdown();

	return 0;
}