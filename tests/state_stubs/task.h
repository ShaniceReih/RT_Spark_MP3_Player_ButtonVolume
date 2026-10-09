#ifndef RT_SPARK_STATE_TEST_TASK_H
#define RT_SPARK_STATE_TEST_TASK_H

#include "FreeRTOS.h"

constexpr BaseType_t taskSCHEDULER_NOT_STARTED = 0;
constexpr BaseType_t taskSCHEDULER_RUNNING = 1;
constexpr BaseType_t taskSCHEDULER_SUSPENDED = 2;

BaseType_t xTaskGetSchedulerState();
TaskHandle_t xTaskGetCurrentTaskHandle();
void vTaskSuspendAll();
BaseType_t xTaskResumeAll();

#endif
