#pragma once

#include "Dune/Graphics/RHI/Resource.h"

namespace Dune::Graphics
{
	enum class EShaderStage : dU8
	{
		Vertex,
		Pixel,
		Compute,
	};

	struct ShaderDesc
	{
		EShaderStage    stage;
		const wchar_t*  filePath{ nullptr };
		const wchar_t*  entryFunc{ nullptr };
		// TODO : use span
		const wchar_t** args;
		dU32            argsCount;
	};

	class Shader : public Resource
	{
		friend class ShaderCompiler;
	public:
		void Initialize(const ShaderDesc& desc);
		void Destroy();
	};

	class ShaderCompiler : public Resource
	{
	public:
		void Initialize();
		Shader CompileShader(const ShaderDesc& desc);
		void Destroy();
	};
}
