//
// Created by lucas on 12/03/2026.
//

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "config.h"

#if defined(ARDUINO)

#define DEBUG_BEGIN(baud)       \
    do                          \
    {                           \
        if (cfg::DEBUG_SERIAL)  \
        {                       \
            Serial.begin(baud); \
        }                       \
    } while (0)

#if 1
#define DEBUG_PRINT(x)         \
    do                         \
    {                          \
        if (cfg::DEBUG_SERIAL) \
        {                      \
            Serial.print(x);   \
        }                      \
    } while (0)

#define DEBUG_PRINTLN(x)       \
    do                         \
    {                          \
        if (cfg::DEBUG_SERIAL) \
        {                      \
            Serial.println(x); \
        }                      \
    } while (0)
#endif

#define DEBUG_VERBOSE_PRINT(x)                       \
    do                                               \
    {                                                \
        if (cfg::DEBUG_SERIAL && cfg::DEBUG_VERBOSE) \
        {                                            \
            Serial.print(x);                         \
        }                                            \
    } while (0)

#define DEBUG_VERBOSE_PRINTLN(x)                     \
    do                                               \
    {                                                \
        if (cfg::DEBUG_SERIAL && cfg::DEBUG_VERBOSE) \
        {                                            \
            Serial.println(x);                       \
        }                                            \
    } while (0)

#else
#define DEBUG_BEGIN(baud)
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_VERBOSE_PRINT(x)
#define DEBUG_VERBOSE_PRINTLN(x)
#endif

#endif // DEBUG_H