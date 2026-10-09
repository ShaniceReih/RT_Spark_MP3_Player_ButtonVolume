#ifndef RT_SPARK_STATE_TEST_SEMPHR_H
#define RT_SPARK_STATE_TEST_SEMPHR_H

#include "FreeRTOS.h"

struct StateTestMutex;
using SemaphoreHandle_t = StateTestMutex *;

SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex);

#endif
