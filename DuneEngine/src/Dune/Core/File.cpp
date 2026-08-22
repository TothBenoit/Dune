#include "pch.h"
#include "Dune/Core/File.h"
#include "Dune/Core/Logger.h"
#include <windows.h>

namespace Dune
{
	constexpr DWORD ToAccessMode(File::EAccessMode access) 
	{
		switch (access)
		{
		case File::EAccessMode::Read:
			return GENERIC_READ;
		case File::EAccessMode::Write:
			return GENERIC_WRITE;
		case File::EAccessMode::ReadWrite:
			return GENERIC_READ | GENERIC_WRITE;
		default:
			Assert(false);
		}
		return GENERIC_READ;
	}

	constexpr DWORD ToShareMode(File::EShareMode share)
	{
		switch (share)
		{
		case File::EShareMode::None:
			return 0;
		case File::EShareMode::Read:
			return FILE_SHARE_READ;
		case File::EShareMode::Write:
			return FILE_SHARE_WRITE;
		case File::EShareMode::ReadWrite:
			return FILE_SHARE_READ | FILE_SHARE_WRITE;
		default:
			Assert(false);
		}
		return 0;
	}

	constexpr DWORD ToSeekMode(File::ESeekMode seek)
	{
		switch (seek)
		{
		case File::ESeekMode::Begin:
			return FILE_BEGIN;
		case File::ESeekMode::Current:
			return FILE_CURRENT;
		case File::ESeekMode::End:
			return FILE_END;
		default:
			Assert(false);
		}
		return 0;
	}

	bool File::Open(File& outFile, const char* filename, EAccessMode access, EShareMode share)
	{
		HANDLE handle = CreateFileA(filename, ToAccessMode(access), ToShareMode(share), NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		outFile.m_pFile = handle;
		return handle != INVALID_HANDLE_VALUE;
	}

	bool File::Read(void* pBuffer, dU64 byteSize)
	{
		dU64 totalBytesRead = 0;
		while (totalBytesRead < byteSize) {
			DWORD bytesRead = 0;
			dU64 bytesWanted = byteSize - totalBytesRead;
			if (!ReadFile(m_pFile, reinterpret_cast<dU8*>(pBuffer) + totalBytesRead, bytesWanted > 0xFFFFFFFF ? 0xFFFFFFFF : (dU32)bytesWanted, &bytesRead, NULL))
				return false;
			totalBytesRead += bytesRead;
		}
		return true;
	}

	bool File::Write(void* pData, dU64 byteSize)
	{
		dU64 totalBytesWritten = 0;
		while (totalBytesWritten < byteSize) {
			DWORD byteWritten = 0;
			dU64 bytesWanted = byteSize - totalBytesWritten;
			if (!WriteFile(m_pFile, reinterpret_cast<dU8*>(pData) + totalBytesWritten, bytesWanted > 0xFFFFFFFF ? 0xFFFFFFFF : (dU32)bytesWanted, &byteWritten, NULL))
				return false;
			totalBytesWritten += byteWritten;
		}
		return true;
	}

	void File::Seek(dU64 byteSize, ESeekMode mode)
	{
		LARGE_INTEGER move;
		move.QuadPart = byteSize;
		SetFilePointerEx(m_pFile, move, NULL, ToSeekMode(mode));
	}

	dU64 File::Tell()
	{
		LARGE_INTEGER pos;
		SetFilePointerEx(m_pFile, {0}, &pos, FILE_CURRENT);
		return pos.QuadPart;
	}

	dU64 File::GetByteSize()
	{
		LARGE_INTEGER size;
		GetFileSizeEx(m_pFile, &size);
		return size.QuadPart;
	}

	bool File::Close()
	{
		return CloseHandle(m_pFile);
	}
}
