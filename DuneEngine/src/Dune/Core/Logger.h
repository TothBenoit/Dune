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
		Logger();
		~Logger();

		enum class LogLevel
		{
			Info,
			Warning,
			Error,
			Critical,
		};

		void PushLog(LogLevel level, const dString& msg);

		void Log(LogLevel level, const dString& msg);
		void Update();

	private:
		static Logger* ms_instance;
		std::thread m_logThread;
		std::mutex m_logMutex;
		//TODO : Lock free queue
		std::queue < std::pair<LogLevel, dString>> m_pendingMessages;
		bool m_shouldProcess;
	};

#define LOG_INFO(msg)		Logger::Info(msg);
#define LOG_WARNING(msg)	Logger::Warning(msg);
#define LOG_ERROR(msg)		Logger::Error(msg);
#define LOG_CRITICAL(msg)	Logger::Critical(msg);
}



