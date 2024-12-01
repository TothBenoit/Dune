#pragma once

#include "Dune/Core/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	enum class EShaderStage
	{
		Vertex,
		Pixel,
		Compute,
	};

	struct ShaderDesc
	{
		EShaderStage	stage;
		const wchar_t*	filePath{ nullptr };
		const wchar_t*	entryFunc{ nullptr };
		// TODO : use span
		const wchar_t** args;
		dU32			argsCount;
	};

	class Shader : public Resource
	{
	public:
		void Initialize(const ShaderDesc& desc);
		void Destroy();
	};
}