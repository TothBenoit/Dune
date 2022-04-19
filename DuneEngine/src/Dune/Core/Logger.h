#pragma once

#include "Core.h"

namespace Dune
{
	class DUNE_API Logger
	{
	public:
		static void Info(const char* msg);
		static void Warning(const char* msg);
		static void Error(const char* msg);
		static void Critical(const char* msg);
	private:
		enum class LogLevel
		{
			Info,
			Warning,
			Error,
			Critical,
		};

		static void Log(LogLevel level, const char* msg);
	};


#define LOG_INFO(msg)		Logger::Info(msg);
#define LOG_WARNING(msg)	Logger::Warning(msg);
#define LOG_ERROR(msg)		Logger::Error(msg);
#define LOG_CRITICAL(msg)	Logger::Critical(msg);
}



