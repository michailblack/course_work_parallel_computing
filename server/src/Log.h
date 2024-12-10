#pragma once
#include <spdlog/spdlog.h>

#include <Windows.h>

class Log
{
public:
    enum class Level
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
    };

public:
    static void                            Init();
    static std::shared_ptr<spdlog::logger> GetLogger() { return s_Logger; }

    template <typename... Args>
    static void PrintMessageWithTag(Log::Level level, std::string_view tag, std::format_string<Args...> format, Args&&... args);

    template <typename... Args>
    static void PrintAssertMessage(std::string_view prefix, std::format_string<Args...> message, Args&&... args);

    static void PrintAssertMessage(std::string_view prefix);

private:
    inline static std::shared_ptr<spdlog::logger> s_Logger{ nullptr };
};

template <typename... Args>
void Log::PrintMessageWithTag(Log::Level level, std::string_view tag, std::format_string<Args...> format, Args&&... args)
{
    auto        logger{ GetLogger() };
    std::string formatted{ std::format(format, std::forward<Args>(args)...) };

    switch (level)
    {
        case Log::Level::Trace:
            logger->trace("[{0}] {1}", tag, formatted);
            break;
        case Log::Level::Debug:
            logger->debug("[{0}] {1}", tag, formatted);
            break;
        case Log::Level::Info:
            logger->info("[{0}] {1}", tag, formatted);
            break;
        case Log::Level::Warn:
            logger->warn("[{0}] {1}", tag, formatted);
            break;
        case Log::Level::Error:
            logger->error("[{0}] {1}", tag, formatted);
            break;
        case Log::Level::Critical:
            logger->critical("[{0}] {1}", tag, formatted);
            break;
    }
}

template <typename... Args>
void Log::PrintAssertMessage(std::string_view prefix, std::format_string<Args...> message, Args&&... args)
{
    auto        logger{ Log::GetLogger() };
    std::string formatted{ std::format(message, std::forward<Args>(args)...) };
    logger->error("{0}: {1}", prefix, formatted);

    MessageBoxA(nullptr, formatted.c_str(), "Assert", MB_OK | MB_ICONERROR);
}

#define LOG_TRACE_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Trace, tag, __VA_ARGS__)
#define LOG_DEBUG_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Debug, tag, __VA_ARGS__)
#define LOG_INFO_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Info, tag, __VA_ARGS__)
#define LOG_WARN_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Warn, tag, __VA_ARGS__)
#define LOG_ERROR_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Error, tag, __VA_ARGS__)
#define LOG_CRITICAL_TAG(tag, ...) ::Log::PrintMessageWithTag(::Log::Level::Critical, tag, __VA_ARGS__)