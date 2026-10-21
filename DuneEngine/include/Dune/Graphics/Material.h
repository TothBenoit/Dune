#pragma once
#include <Dune/Resources/Shaders/ShaderInterop.h>
#include <Dune/Platform/RHI/PipelineState.h>
#include <Dune/Graphics/Handles.h>

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

	struct MaterialDesc
	{
		dVec3         baseColor{ 1.0f, 1.0f, 1.0f };
		float         metalness{ 0.0f };
		float         roughness{ 0.5f };
		float         alphaCutoff{ 0.5f };
		EAlphaMode    alphaMode{ EAlphaMode::Opaque };
		RHI::ECullingMode  faceCulling{ RHI::ECullingMode::Back };
		TextureHandle albedo;
		TextureHandle normal;
		TextureHandle roughnessMetalness;
	};

	using MaterialKey = dU32;
	struct Material
	{
		static constexpr dU32 kKeyTableSize = (dU32)EAlphaMode::Count * (dU32)RHI::ECullingMode::Count;

		[[nodiscard]] static constexpr dU32 MakeKey(EAlphaMode alphaMode, RHI::ECullingMode faceCulling) { return (dU32)faceCulling + (dU32)alphaMode * (dU32)RHI::ECullingMode::Count; }
		[[nodiscard]] static constexpr EAlphaMode GetAlphaMode(MaterialKey key) { return (EAlphaMode)(key / (dU32)RHI::ECullingMode::Count); }
		[[nodiscard]] static constexpr RHI::ECullingMode GetFaceCulling(MaterialKey key) { return (RHI::ECullingMode)(key % (dU32)RHI::ECullingMode::Count); }

		[[nodiscard]] dU32 GetKey() const { return MakeKey(alphaMode, faceCulling); }

		MaterialData shaderData;
		EAlphaMode alphaMode{ EAlphaMode::Opaque };
		RHI::ECullingMode faceCulling{ RHI::ECullingMode::Back };
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
