#pragma once
#include <Dune/Graphics/Material.h>

namespace Dune::Graphics
{
	struct ViewData
	{
		dMatrix4x4 view;
		dMatrix4x4 projection;
		dMatrix4x4 viewProjection;
		dVec3 position;
	};

	struct DrawItem
	{
		dMatrix4x4 objectToWorld;
		dU32 meshIdx;
		dU32 materialIdx;
		dU32 indexOffset;
		dU32 indexCount;
		dU32 vertexOffset;
		MaterialKey materialKey;
	};

	struct GPUMeshView
	{
		dU64 indicesGPUAddress;
		dU32 indicesByteSize;
		bool indicesAre32Bit;
		dU64 verticesGPUAddress;
		dU32 verticesByteSize;
		dU32 verticesByteStride;
	};

	struct FrameData
	{
		dVector<LightData> lights;
		dVector<DrawItem> drawItems;
		dVector<GPUMeshView> meshes;
		dVector<MaterialData> materials;
		dU64 sharedSRVHeapCPUAddress;
		dU32 sharedSRVHeapCapacity;
		float deltaTime{ 0.0f };
	};
}
