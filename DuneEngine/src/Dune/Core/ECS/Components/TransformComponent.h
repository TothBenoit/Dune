#pragma once

namespace Dune
{
	// TODO : should be a single matrix, and should be only modified through a system so we update matrix only when it changed (same as camera)
	struct TransformComponent
	{
		dVec3 position = { 0.f,0.f,0.f };
		dVec3 rotation = { 0.f, 0.f, 0.f };
		dVec3 scale = { 1.f,1.f,1.f };
		
		dMatrix matrix;
		bool hasChanged = true;
	};
}