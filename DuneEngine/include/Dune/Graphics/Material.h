#pragma once
#include <Dune/Resources/Shaders/ShaderInterop.h>

namespace Dune::Graphics
{
	enum class  EAlphaMode : dU8
	{
		Opaque,
		Mask,
		Blend,
		Count
	};

	struct Material
	{
		MaterialData shaderData;
		EAlphaMode alphaMode{ EAlphaMode::Opaque };
		bool isDoubleSided{ false };

		[[nodiscard]] inline dU32 GetVariant() const { return (dU32)alphaMode * 2 + (isDoubleSided ? 1u : 0u); }

		static inline constexpr dU32 kVariantCount = (dU32)EAlphaMode::Count * 2;
		static inline constexpr dU32 kDepthVariantCount = 4;

		static inline EAlphaMode GetAlphaMode(dU32 variant) { return (EAlphaMode)(variant >> 1); }
		static inline bool       IsDoubleSided(dU32 variant) { return (variant & 1) != 0; }

		static_assert((dU32)EAlphaMode::Blend == (dU32)EAlphaMode::Count - 1, "Blend must sort last");
		static_assert(kDepthVariantCount == (dU32)EAlphaMode::Blend * 2, "Depth variants must cover every non-blend mode");
	};
}
