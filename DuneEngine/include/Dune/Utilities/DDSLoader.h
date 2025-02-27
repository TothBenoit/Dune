#pragma once

#include "Dune/Graphics/Format.h"

namespace Dune::Graphics
{

    class Device;
    class Texture;
    class CommandList;
    class Buffer;

    enum class DDSResult
    {
        ESucceed,
        EFailedOpen,
        EFailedRead,
        EFailedMagicWord,
        EFailedSize,
    };

    enum class DDSTextureDimension : dU32 {
        EUnknown = 0,
        ETexture1D = 2,
        ETexture2D = 3,
        ETexture3D = 4
    };

    struct DDSHeaderDXT10 {
        Graphics::EFormat format;
        DDSTextureDimension resourceDimension;
        dU32 miscFlag;
        dU32 arraySize;
        dU32 miscFlag2;
    };

    enum class DDSPixelFormatFlagBits : dU32
    {
        FourCC = 0x00000004,
        RGB = 0x00000040,
        RGBA = 0x00000041,
        Luminance = 0x00020000,
        LuminanceA = 0x00020001,
        AlphaPixels = 0x00000001,
        Alpha = 0x00000002,
        Palette8 = 0x00000020,
        Palette8A = 0x00000021,
        BumpDUDV = 0x00080000
    };

    struct DDSPixelFormat {
        dU32 size;
        dU32 flags;
        dU32 fourCC;
        dU32 bitCount;
        dU32 RBitMask;
        dU32 GBitMask;
        dU32 BBitMask;
        dU32 ABitMask;
    };

	struct DDSHeader
	{
        dU32 size;
        dU32 flags;
        dU32 height;
        dU32 width;
        dU32 pitchOrLinearSize;
        dU32 depth;
        dU32 mipMapCount;
        dU32 reserved1[11];
        DDSPixelFormat pixelFormat;
        dU32 caps;
        dU32 caps2;
        dU32 caps3;
        dU32 caps4;
        dU32 reserved2;
	};

	class DDSTexture 
	{
	public:

		static DDSResult Load(const char* filePath, DDSTexture& outDDSTexture);
        static Graphics::Texture* CreateTextureFromFile(Device* pDevice, CommandList* pCommandList, Buffer& uploadBuffer, const char* filePath);

        DDSResult Load(const char* filePath);
        void Destroy();

        void* GetData() { return m_pData; };
        const DDSHeader* GetHeader() const { return m_pHeader; };
        const DDSHeaderDXT10* GetHeaderDXT10() const { return m_pHeaderDXT10; };

	private:
        void* m_pFileBuffer{ nullptr };
        DDSHeader* m_pHeader{ nullptr };
        DDSHeaderDXT10* m_pHeaderDXT10{ nullptr };
        void* m_pData{ nullptr };
	};

}