#pragma once

namespace Dune::System::IO
{
	enum class EShareMode
	{
		None          = 0,
		Read          = 1 << 0,
		Write         = 1 << 1,
		ReadWrite     = Read | Write
	};

	enum class EAccessMode
	{
		Read          = 1 << 0,
		Write         = 1 << 1,
		ReadWrite     = Read | Write,
	};

	enum class ESeekMode
	{
		Begin         = 0,
		Current       = 1,
		End           = 2
	};

	class File
	{
	public:
		static bool Open(File& outFile, const char* filename, EAccessMode access, EShareMode share);
		bool Read(void* pBuffer, dU32 byteSize);
		bool Write(void* pData, dU32 byteSize);
		void Seek(dU64 bytesOffset, ESeekMode mode);
		dU64 Tell();
		dU64 GetByteSize();
		bool Close();

	private:
		void* m_pFile{ nullptr };
	};
}