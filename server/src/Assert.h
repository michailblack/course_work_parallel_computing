#pragma once

#ifdef ENABLE_ASSERTS
    #define ASSERT(condition, ...)                                                        \
        {                                                                                 \
            if (!(condition))                                                             \
            {                                                                             \
                ::Log::PrintAssertMessage("Assertion Failed" __VA_OPT__(, ) __VA_ARGS__); \
                __debugbreak();                                                           \
            }                                                                             \
        }

    #ifdef DEBUG
        #define VERIFY(expr, ...)                                                                \
            {                                                                                    \
                if (!(expr))                                                                     \
                {                                                                                \
                    ::Log::PrintAssertMessage("Verification Failed" __VA_OPT__(, ) __VA_ARGS__); \
                    __debugbreak();                                                              \
                }                                                                                \
            }
    #else
        #define VERIFY(expr, ...) (expr)
    #endif

#else
    #define ASSERT(x, ...)
    #define VERIFY(expr, ...) (expr)
#endif