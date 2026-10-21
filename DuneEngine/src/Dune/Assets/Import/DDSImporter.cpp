#include "pch.h"
#include "Dune/Assets/Import/DDSImporter.h"
#include "Dune/Platform/File.h"
#include "Dune/Platform/Logger.h"

namespace Dune::DDSImporter
{
	namespace
	{
		using Graphics::EFormat;

		enum class DDSTextureDimension : dU32 {
			EUnknown    = 0,
			ETexture1D  = 2,
			ETexture2D  = 3,
			ETexture3D  = 4
		};

		struct DDSHeaderDXT10 {
			EFormat format;
			DDSTextureDimension resourceDimension;
			dU32 miscFlag;
			dU32 arraySize;
			dU32 miscFlag2;
		};

		enum class DDSPixelFormatFlagBits : dU32
		{
			FourCC      = 0x00000004,
			RGB         = 0x00000040,
			RGBA        = 0x00000041,
			Luminance   = 0x00020000,
			LuminanceA  = 0x00020001,
			AlphaPixels = 0x00000001,
			Alpha       = 0x00000002,
			Palette8    = 0x00000020,
			Palette8A   = 0x00000021,
			BumpDUDV    = 0x00080000
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

		constexpr dU32 kCubemapFlag{ 0x200 };

		constexpr dU32 MakeFourCC(char ch0, char ch1, char ch2, char ch3)
		{
			return (dU32(dU8(ch0)) | (dU32(dU8(ch1)) << 8) | (dU32(dU8(ch2)) << 16) | (dU32(dU8(ch3)) << 24));
		}

		constexpr bool HasBitMask(DDSPixelFormat pixelFormat, dU32 r, dU32 g, dU32 b, dU32 a)
		{
			return pixelFormat.RBitMask == r && pixelFormat.GBitMask == g && pixelFormat.BBitMask == b && pixelFormat.ABitMask == a;
		}

		EFormat ToFormat(const DDSPixelFormat& pixelFormat)
		{
			switch (pixelFormat.fourCC)
			{
			case MakeFourCC('B', 'C', '4', 'U'): return EFormat::BC4_UNORM;
			case MakeFourCC('D', 'X', 'T', '1'): return EFormat::BC1_UNORM;
			case MakeFourCC('D', 'X', 'T', '3'): return EFormat::BC2_UNORM;
			case MakeFourCC('D', 'X', 'T', '5'): return EFormat::BC3_UNORM;
			case MakeFourCC('B', 'C', '5', 'U'): return EFormat::BC5_UNORM;
			case MakeFourCC('A', 'T', 'I', '2'): return EFormat::BC5_UNORM;
			case 0:
				if (HasBitMask(pixelFormat, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
					return EFormat::R8G8B8A8_UNORM;
				if (HasBitMask(pixelFormat, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
					return EFormat::B8G8R8A8_UNORM;
				return EFormat::Unknown;
			default:
				return EFormat::Unknown;
			}
		}

		bool Fail(File& file, const char* filePath, const char* reason)
		{
			file.Close();
			LOG_ERROR(StringUtils::printf("DDSImporter: %s (%s)", reason, filePath).c_str());
			return false;
		}
	}

	bool Import(const char* filePath, Graphics::TextureData& outImage)
	{
		File file;
		if (!File::Open(file, filePath, File::EAccessMode::Read, File::EShareMode::None))
		{
			LOG_ERROR(StringUtils::printf("DDSImporter: cannot open file (%s)", filePath).c_str());
			return false;
		}

		const dU64 fileByteSize = file.GetByteSize();
		dU32 magicWord{ 0 };
		DDSHeader header{};
		if (fileByteSize < sizeof(magicWord) + sizeof(DDSHeader) || !file.Read(&magicWord, sizeof(magicWord)) || !file.Read(&header, sizeof(DDSHeader)))
			return Fail(file, filePath, "file too small");
		if (magicWord != MakeFourCC('D', 'D', 'S', ' '))
			return Fail(file, filePath, "not a DDS file");
		if (header.caps2 & kCubemapFlag)
			return Fail(file, filePath, "cubemaps are not supported");

		dU64 dataOffset = sizeof(magicWord) + sizeof(DDSHeader);
		EFormat format{ EFormat::Unknown };
		if ((header.pixelFormat.flags & dU32(DDSPixelFormatFlagBits::FourCC)) && header.pixelFormat.fourCC == MakeFourCC('D', 'X', '1', '0'))
		{
			DDSHeaderDXT10 headerDXT10{};
			if (fileByteSize < dataOffset + sizeof(DDSHeaderDXT10) || !file.Read(&headerDXT10, sizeof(DDSHeaderDXT10)))
				return Fail(file, filePath, "file too small");
			if (headerDXT10.resourceDimension != DDSTextureDimension::ETexture2D || headerDXT10.arraySize > 1)
				return Fail(file, filePath, "only single 2D textures are supported");
			dataOffset += sizeof(DDSHeaderDXT10);
			format = headerDXT10.format;
		}
		else
		{
			format = ToFormat(header.pixelFormat);
		}

		if (format == EFormat::Unknown)
			return Fail(file, filePath, "unsupported pixel format");

		outImage.format = format;
		outImage.width = header.width;
		outImage.height = header.height;
		outImage.mipLevels = std::max(header.mipMapCount, 1u);
		outImage.bytes.resize(fileByteSize - dataOffset);
		if (!file.Read(outImage.bytes.data(), outImage.bytes.size()))
			return Fail(file, filePath, "cannot read pixel data");

		file.Close();
		return true;
	}
}
