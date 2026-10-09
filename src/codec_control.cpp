#include "codec_control.h"

#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

namespace {
SemaphoreHandle_t codecMutex = nullptr;
}

void CodecControl_Init()
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    configASSERT(codecMutex == nullptr);
    codecMutex = xSemaphoreCreateMutex();
    configASSERT(codecMutex != nullptr);
}

void CodecControl_Lock()
{
    configASSERT(__get_IPSR() == 0U);
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(codecMutex != nullptr);
    configASSERT(xSemaphoreTake(codecMutex, portMAX_DELAY) == pdTRUE);
}

void CodecControl_Unlock()
{
    configASSERT(__get_IPSR() == 0U);
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return;
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(codecMutex != nullptr);
    configASSERT(xSemaphoreGive(codecMutex) == pdTRUE);
}

#endif
