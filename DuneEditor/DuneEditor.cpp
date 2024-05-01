#include <Dune.h>
#include <imgui/imgui.h>

using namespace Dune;

struct TestComponent
{
	int a;
	int b;
};

int main(int argc, char** argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	return 0;
}