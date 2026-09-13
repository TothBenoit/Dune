#pragma once
#include <Dune/Resources/Shaders/ShaderInterop.h>
#include <Dune/Graphics/RHI/PipelineState.h>

namespace Dune::Graphics
{
	enum class EAlphaMode : dU8
	{
		Opaque,
		Mask,
		Blend,
		Count
	};
	static_assert((dU32)EAlphaMode::Blend == (dU32)EAlphaMode::Count - 1, "Blend must sort last");

	using MaterialKey = dU32;
	struct Material
	{
		static constexpr dU32 kKeyTableSize = (dU32)EAlphaMode::Count * (dU32)ECullingMode::Count;

		[[nodiscard]] static constexpr dU32 MakeKey(EAlphaMode alphaMode, ECullingMode faceCulling) { return (dU32)faceCulling + (dU32)alphaMode * (dU32)ECullingMode::Count; }
		[[nodiscard]] static constexpr EAlphaMode GetAlphaMode(MaterialKey key) { return (EAlphaMode)(key / (dU32)ECullingMode::Count); }
		[[nodiscard]] static constexpr ECullingMode GetFaceCulling(MaterialKey key) { return (ECullingMode)(key % (dU32)ECullingMode::Count); }

		[[nodiscard]] dU32 GetKey() const { return MakeKey(alphaMode, faceCulling); }

		MaterialData shaderData;
		EAlphaMode alphaMode{ EAlphaMode::Opaque };
		ECullingMode faceCulling{ ECullingMode::Back };
	};

	[[nodiscard]] static constexpr bool ValidateMaterialKeys()
	{
		for (dU32 k = 0; k < Material::kKeyTableSize; k++)
			if (Material::MakeKey(Material::GetAlphaMode(k), Material::GetFaceCulling(k)) != k)
				return false;
		return true;
	}
	static_assert(ValidateMaterialKeys());
}
