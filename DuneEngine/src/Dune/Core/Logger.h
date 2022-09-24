#pragma once

namespace Dune
{
	class Logger
	{
	public:
		static void Init();
		static void Shutdown();
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
		static void Log(LogLevel level, const dString& msg);
		static void Update();
	private:
		static std::thread m_logThread;
		static std::mutex m_logMutex;
		//Use LockLessQueue
		static std::queue < std::pair<LogLevel, dString>> m_pendingMessages;
		static std::atomic_bool m_shouldProcess;
	};

#define LOG_INFO(msg)		Logger::Info(msg);
#define LOG_WARNING(msg)	Logger::Warning(msg);
#define LOG_ERROR(msg)		Logger::Error(msg);
#define LOG_CRITICAL(msg)	Logger::Critical(msg);
}



