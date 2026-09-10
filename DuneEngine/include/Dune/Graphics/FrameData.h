#pragma once
#include <Dune/Resources/Shaders/ShaderInterop.h>

namespace Dune::Graphics
{
	struct FrameLights
	{
		dVector<Light>      allActive;
		dVector<dU32>       shadowCasters;
		dVector<dMatrix4x4> shadowMatrices;
	};

	struct DrawItem
	{
		dMatrix4x4 objectToWorld;
		dU32 meshIdx;
		dU32 materialIdx;
		dU32 indexOffset;
		dU32 indexCount;
		dU32 vertexOffset;
		dU32 materialVariant;
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
		FrameLights lights;
		dVector<GPUMeshView> meshes;
		dVector<MaterialData> materials;
		dVector<DrawItem> drawItems;
		dU64 sharedSRVHeapCPUAddress;
		dU32 sharedSRVHeapCapacity;
		dU32 blendDrawCount;
	};
}
