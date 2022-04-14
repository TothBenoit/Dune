#pragma once

#include "Core.h"
#include <string>

namespace Dune
{
	class DUNE_API Logger
	{
	public:
		static void Info(const std::string& msg);
		static void Warning(const std::string& msg);
		static void Error(const std::string& msg);
		static void Critical(const std::string& msg);
	private:
		enum class LogLevel
		{
			Info,
			Warning,
			Error,
			Critical,
		};

		static void Log(LogLevel level, const std::string& msg);
	};


#define LOG_INFO(msg)		Logger::Info(msg);
#define LOG_WARNING(msg)	Logger::Warning(msg);
#define LOG_ERROR(msg)		Logger::Error(msg);
#define LOG_CRITICAL(msg)	Logger::Critical(msg);
}



