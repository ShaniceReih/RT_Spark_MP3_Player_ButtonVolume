#ifndef RT_SPARK_STATE_TEST_FREERTOS_H
#define RT_SPARK_STATE_TEST_FREERTOS_H

#include <cstdint>

using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using TaskHandle_t = void *;

constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
constexpr TickType_t portMAX_DELAY = 0xFFFFFFFFU;

namespace StateTestRtos {
extern uint32_t interruptNumber;
void AssertFailed(const char *condition);
}

inline uint32_t __get_IPSR() { return StateTestRtos::interruptNumber; }
#define configASSERT(condition) \
    do { if (!(condition)) StateTestRtos::AssertFailed(#condition); } while (0)

#endif
