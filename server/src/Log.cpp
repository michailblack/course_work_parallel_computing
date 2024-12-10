#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

void Log::Init()
{
    spdlog::set_pattern("%^[%T] [%t] %n: %v%$");

#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    s_Logger = spdlog::stdout_color_mt("SERVER");
}

void Log::PrintAssertMessage(std::string_view prefix)
{
    auto logger{ Log::GetLogger() };
    logger->error("{0}", prefix);

    MessageBoxA(nullptr, "No message :/", "Assert", MB_OK | MB_ICONERROR);
}
