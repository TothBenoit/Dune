#pragma once

namespace Dune
{
	struct TransformComponent
	{
		dVec3 position { 0.f,0.f,0.f };
		dVec3 rotation { 0.f, 0.f, 0.f };
		dVec3 scale { 1.f,1.f,1.f };
		
		dMatrix matrix;
	};
}